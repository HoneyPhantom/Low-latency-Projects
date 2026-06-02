#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <string>
#include <thread>
#include <stdexcept>
#include <cerrno>
#include <atomic>

// Network & Socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

// Apple Silicon Mac Core Locking Headers
#include <pthread.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>

#pragma pack(push, 1)
struct MarketDataPacket {
    char type;         
    uint32_t order_id; 
    uint32_t qty;      
    int price;         
    char side;         
};
#pragma pack(pop)

static constexpr uint32_t NULL_IDX = 0xFFFFFFFF;
static constexpr size_t CHUNK_SIZE = 64; 
static constexpr size_t QUEUE_CAPACITY = 512;

struct alignas(64) DataChunk {
    MarketDataPacket packets[CHUNK_SIZE];
    size_t valid_count = 0;
};

template<typename T, size_t Capacity>
class ZeroCopySPSC {
private:
    static_assert((Capacity & (Capacity - 1)) == 0 , "Capacity must be power of 2");
    alignas(64) T buffer[Capacity];
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};

    inline void return_read_cache() { read_index_cached = read_idx.load(std::memory_order_acquire); }
    inline void return_write_cache() { write_index_cached = write_idx.load(std::memory_order_acquire); }

    alignas(64) size_t read_index_cached = 0;
    alignas(64) size_t write_index_cached = 0;

public:
    inline T* get_write_slot() {
        const size_t curr_write = write_idx.load(std::memory_order_relaxed);
        const size_t curr_read = read_index_cached;
        
        if ((curr_write - curr_read) == Capacity) [[unlikely]] {
            return_read_cache();
            if ((curr_write - read_index_cached) == Capacity) return nullptr;
        }
        return &buffer[curr_write & (Capacity - 1)];
    }

    inline void commit_write() {
        size_t curr_write = write_idx.load(std::memory_order_relaxed);
        write_idx.store(curr_write + 1, std::memory_order_release);
    }

    inline const T* peek_read_slot() {
        const size_t curr_read = read_idx.load(std::memory_order_relaxed);
        const size_t curr_write = write_index_cached;

        if (curr_read == curr_write) {
            return_write_cache();
            if (curr_read == write_index_cached) return nullptr;
        }
        return &buffer[curr_read & (Capacity - 1)];
    }

    inline void commit_read() {
        size_t curr_read = read_idx.load(std::memory_order_relaxed);
        read_idx.store(curr_read + 1, std::memory_order_release);
    }
};

inline void hardware_spin_relax() {
#if defined(__ARM_ARCH) || defined(__aarch64__)
    asm volatile("isb" ::: "memory");
#else
    asm volatile("pause" ::: "memory");
#endif
}

template<typename T , size_t POOL_SIZE>
class MemoryPool {
private:
    alignas(64) char buffer[POOL_SIZE * sizeof(T)];
    struct FreeNode { uint32_t next_idx; };
    uint32_t free_head = 0;
    
    inline T* get_slot(size_t idx) { return reinterpret_cast<T*>(&buffer[idx * sizeof(T)]); }
    inline const T* get_slot(size_t idx) const { return reinterpret_cast<const T*>(&buffer[idx * sizeof(T)]); }
public:
    MemoryPool() {
        for(size_t i = 0; i < POOL_SIZE - 1; ++i) reinterpret_cast<FreeNode*>(get_slot(i))->next_idx = static_cast<uint32_t>(i + 1);
        reinterpret_cast<FreeNode*>(get_slot(POOL_SIZE - 1))->next_idx = static_cast<uint32_t>(NULL_IDX);
    }

    uint32_t alloc_raw(){
        if(free_head == NULL_IDX) [[unlikely]] return NULL_IDX;
        uint32_t node = free_head;
        free_head = reinterpret_cast<FreeNode*>(get_slot(node))->next_idx;
        return node;
    }

    void dealloc_raw(uint32_t idx){
        if(idx == NULL_IDX || idx >= POOL_SIZE) return;
        reinterpret_cast<FreeNode*>(get_slot(idx))->next_idx = free_head;
        free_head = idx;
    }

    template <class... Args>
    uint32_t create(Args&&... args) {
        uint32_t idx = alloc_raw();
        if (idx == NULL_IDX) return NULL_IDX;
        new (get_slot(idx)) T(std::forward<Args>(args)...);
        return idx;
    }

    void destroy(uint32_t idx) {
        if(idx == NULL_IDX || idx >= POOL_SIZE) return;
        get_slot(idx)->~T();
        dealloc_raw(idx);
    }

    inline T& operator[](size_t idx) { return const_cast<T&>(static_cast<const MemoryPool&>(*this)[idx]); }
    inline const T& operator[](size_t idx) const { return *get_slot(idx); }
    inline T* get_base_ptr() { return reinterpret_cast<T*>(buffer); }
};

struct Order {
    uint32_t id;
    uint32_t qty;
    int price;
    uint32_t next_idx;
    Order(uint32_t id_, uint32_t qty_, int price_) : id(id_), qty(qty_), price(price_), next_idx(NULL_IDX) {}
};

struct Level {
    uint32_t head = NULL_IDX;
    uint32_t tail = NULL_IDX;

    inline void push(Order* pool , uint32_t new_order_idx){
        pool[new_order_idx].next_idx = NULL_IDX;
        if(tail == NULL_IDX) head = tail = new_order_idx;
        else {
            pool[tail].next_idx = new_order_idx;
            tail = new_order_idx;
        }
    }

    inline uint32_t pop(Order* pool){
        if(head == NULL_IDX) return NULL_IDX;
        uint32_t x = head;
        head = pool[x].next_idx;
        if(empty()) tail = NULL_IDX;
        return x;
    }
    inline uint32_t top() const { return head; }
    inline bool empty() const { return head == NULL_IDX; }
};

static constexpr int MIN_PRICE = 900;
static constexpr int MAX_PRICE = 1100;
static constexpr int LEVELS = MAX_PRICE - MIN_PRICE + 1;
inline int idx(int price){ return price - MIN_PRICE; }

class Pricemap {
private:
    uint64_t masks[4] = {0};
public:
    inline void set(int idx) { masks[idx >> 6] |= (1ULL << (idx & 63)); } 
    inline void clear(int idx) { masks[idx >> 6] &= ~(1ULL << (idx & 63)); }
    inline int find_max() const {
        if (masks[3]) return (3 << 6) | (63 - __builtin_clzll(masks[3]));
        if (masks[2]) return (2 << 6) | (63 - __builtin_clzll(masks[2]));
        if (masks[1]) return (1 << 6) | (63 - __builtin_clzll(masks[1]));
        if (masks[0]) return (0 << 6) | (63 - __builtin_clzll(masks[0]));
        return -1;
    }
    inline int find_min() const {
        if (masks[0]) return (0 << 6) | __builtin_ctzll(masks[0]);
        if (masks[1]) return (1 << 6) | __builtin_ctzll(masks[1]);
        if (masks[2]) return (2 << 6) | __builtin_ctzll(masks[2]);
        if (masks[3]) return (3 << 6) | __builtin_ctzll(masks[3]);
        return -1;
    }
};

class alignas(64) OrderBook {
private:
    alignas(64) Level bids[LEVELS];
    alignas(64) Level asks[LEVELS];
    MemoryPool<Order , 1000000> pool;
    uint32_t next_idx = 1;
    Pricemap bid_mask, ask_mask;

    inline uint32_t branchless_min(const uint32_t& a , const uint32_t& b){
        return b + ((a - b) & ((int32_t)(a - b) >> 31));
    }
    inline void enqueue_bid(int price , uint32_t qty){
        uint32_t id = pool.create(next_idx++ , qty , price);
        if(id == NULL_IDX) [[unlikely]] return;
        int price_idx = idx(price);
        bids[price_idx].push(get_raw_pool() , id);
        bid_mask.set(price_idx);
    }
    inline void enqueue_ask(int price , uint32_t qty){
        uint32_t id = pool.create(next_idx++ , qty , price);
        if(id == NULL_IDX) [[unlikely]] return;
        int price_idx = idx(price);
        asks[price_idx].push(get_raw_pool() , id);
        ask_mask.set(price_idx);
    }
    void handle_buy(int price , uint32_t qty){
        Order* raw_pool = get_raw_pool();
        while(qty > 0){
            int best_ask_idx = ask_mask.find_min();
            if(best_ask_idx == -1 || best_ask_idx + MIN_PRICE > price) break;
            Level& lvl = asks[best_ask_idx];
            while(qty > 0 && !lvl.empty()){
                uint32_t id = lvl.top();
                Order& o = pool[id];
                uint32_t traded = branchless_min(o.qty , qty);
                o.qty -= traded;
                qty -= traded;
                if(o.qty == 0){
                    uint32_t done = lvl.pop(raw_pool);
                    pool.destroy(done);
                }
            }
            if(lvl.empty()) ask_mask.clear(best_ask_idx);
        }
        if(qty > 0) enqueue_bid(price , qty);
    }
    void handle_sell(int price , uint32_t qty){
        Order* raw_pool = get_raw_pool();
        while(qty > 0){
            int best_bid_idx = bid_mask.find_max();
            if(best_bid_idx == -1 || best_bid_idx + MIN_PRICE < price) break;
            Level& lvl = bids[best_bid_idx];
            while(qty > 0 && !lvl.empty()){
                uint32_t id = lvl.top();
                Order& o = pool[id];
                uint32_t traded = branchless_min(o.qty , qty);
                o.qty -= traded;
                qty -= traded;
                if(o.qty == 0){
                    uint32_t done = lvl.pop(raw_pool);
                    pool.destroy(done);
                }
            }
            if(lvl.empty()) bid_mask.clear(best_bid_idx);
        }
        if(qty > 0) enqueue_ask(price , qty);
    }
public:
    inline Order* get_raw_pool() { return pool.get_base_ptr(); }
    inline int get_best_bid() const{ 
        int idx = bid_mask.find_max();
        return idx == -1 ? -1 : idx + MIN_PRICE;
    }
    inline int get_best_ask() const{ 
        int idx = ask_mask.find_min();
        return idx == -1 ? -1 : idx + MIN_PRICE; 
    }
    void add_in_limit(bool is_buy , int price , uint32_t qty){
        if(price < MIN_PRICE || price > MAX_PRICE) return;
        if(is_buy) handle_buy(price , qty);
        else handle_sell(price , qty);
    }
};

OrderBook book;
ZeroCopySPSC<DataChunk, QUEUE_CAPACITY> zero_copy_queue;

class MarketDataReceiver {
private:
    int server_fd = -1;
    ZeroCopySPSC<DataChunk, QUEUE_CAPACITY>& queue;
    alignas(64) MarketDataPacket network_buffer[4096]; 
    int active_port = -1;

public:
    MarketDataReceiver(ZeroCopySPSC<DataChunk, QUEUE_CAPACITY>& q_ref, int base_port) : queue(q_ref) {
        server_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (server_fd < 0) throw std::runtime_error("Socket creation failed");

        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

        int rcvbuf_size = 8 * 1024 * 1024;
        setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Automatic Bind Retry Escalation Strategy
        bool bound_successfully = false;
        for (int offset = 0; offset < 20; ++offset) {
            int current_port = base_port + offset;
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(current_port);

            if (::bind(server_fd, (struct sockaddr*)&address, sizeof(address)) >= 0) {
                active_port = current_port;
                bound_successfully = true;
                break;
            }
        }

        if (!bound_successfully) {
            throw std::runtime_error("Socket binding failed: All ports in fallback range are locked.");
        }
    }

    ~MarketDataReceiver() { if (server_fd != -1) close(server_fd); }

    int get_active_port() const { return active_port; }

    void run_receiver_loop(size_t max_packets_to_process) {
        thread_affinity_policy_data_t policy = { 3 };
        thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());
        thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

        size_t packets_received = 0;
        DataChunk* current_slot = nullptr;
        while ((current_slot = queue.get_write_slot()) == nullptr) hardware_spin_relax();
        current_slot->valid_count = 0;

        std::cout << "Engine active. Running Zero-Copy In-Place SPSC Engine on Port " << active_port << "..." << std::endl;

        while (packets_received < max_packets_to_process) {
            ssize_t bytes_read = recv(server_fd, network_buffer, sizeof(network_buffer), 0);
            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue; 
                break;
            }
            size_t packets_in_batch = static_cast<size_t>(bytes_read) / sizeof(MarketDataPacket);
            for (size_t i = 0; i < packets_in_batch; ++i) {
                current_slot->packets[current_slot->valid_count++] = network_buffer[i];
                ++packets_received;
                if (current_slot->valid_count == CHUNK_SIZE) {
                    queue.commit_write(); 
                    while ((current_slot = queue.get_write_slot()) == nullptr) hardware_spin_relax();
                    current_slot->valid_count = 0;
                }
            }
        }
        if (current_slot && current_slot->valid_count > 0) queue.commit_write();
        std::cout << "Target boundary satisfied. Processing halted." << std::endl;
    }
};

void run_matching_engine_consumer(OrderBook& book_obj, ZeroCopySPSC<DataChunk, QUEUE_CAPACITY>& q_ref, size_t total_packets) {
    thread_affinity_policy_data_t policy = { 4 };
    thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

    size_t processed = 0;

    while (processed < total_packets) {
        const DataChunk* current_slot = q_ref.peek_read_slot();
        if (current_slot != nullptr) {
            size_t count = current_slot->valid_count;
            for (size_t i = 0; i < count; ++i) {
                const auto& packet = current_slot->packets[i];
                book_obj.add_in_limit(packet.side == 'B', packet.price, packet.qty);
            }
            processed += count;
            q_ref.commit_read();
        } else {
            hardware_spin_relax();
        }
    }
}

void run_mock_exchange_tx(int port, size_t total_packets) {
    thread_affinity_policy_data_t policy = { 2 };
    thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    std::cout << "Mock Exchange Thread running on Core 2. Injecting " << total_packets << " packets into Port " << port << "..." << std::endl;

    const size_t packets_per_frame = 8; 
    std::vector<MarketDataPacket> frame_buffer(packets_per_frame);

    uint32_t rng = 42;
    auto fast_rng = [&](uint32_t &s) -> uint32_t {
        s = s * 1664525u + 1013904223u;
        return s;
    };

    size_t packets_sent = 0;
    while (packets_sent < total_packets) {
        for (size_t i = 0; i < packets_per_frame; ++i) {
            uint32_t r = fast_rng(rng);
            frame_buffer[i].type = 'Q';
            frame_buffer[i].order_id = static_cast<uint32_t>(packets_sent + i + 1);
            frame_buffer[i].qty = 1 + (r % 20);
            frame_buffer[i].price = MIN_PRICE + (r % LEVELS);
            frame_buffer[i].side = (r & 1) ? 'B' : 'S';
        }

        sendto(client_fd, frame_buffer.data(), packets_per_frame * sizeof(MarketDataPacket), 0,
               (struct sockaddr*)&serv_addr, sizeof(serv_addr));

        packets_sent += packets_per_frame;
    }
    close(client_fd);
    std::cout << "Mock Exchange finished transmission." << std::endl;
}

int main() {
    const int BASE_PORT = 12345;
    const size_t TOTAL_PACKETS = 5000000;
    using clock = std::chrono::steady_clock;

    try {
        MarketDataReceiver receiver(zero_copy_queue, BASE_PORT);
        int resolved_port = receiver.get_active_port();

        std::cout << "Starting Lock-Free ZERO-COPY SPSC Pipeline..." << std::endl;
        std::cout << "IO Network Thread   -> Core 3" << std::endl;
        std::cout << "Matching Engine Core -> Core 4" << std::endl;

        std::thread exchange_thread(run_mock_exchange_tx, resolved_port, TOTAL_PACKETS);
        
        auto start_time = clock::now();

        std::thread engine_thread(run_matching_engine_consumer, std::ref(book), std::ref(zero_copy_queue), TOTAL_PACKETS);
        receiver.run_receiver_loop(TOTAL_PACKETS);
        
        if (engine_thread.joinable()) engine_thread.join();
        auto end_time = clock::now();
        
        if (exchange_thread.joinable()) exchange_thread.join();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        std::cout << "\n================ ENGINE RUN STATS ================" << std::endl;
        std::cout << "Total Data Streamed    : " << TOTAL_PACKETS << " Packets" << std::endl;
        std::cout << "Total Processing Time  : " << duration << " microseconds" << std::endl;
        std::cout << "Average Cost Per Packet: " << (double)duration * 1000.0 / TOTAL_PACKETS << " nanoseconds" << std::endl;
        std::cout << "Final Matching State   : Best Bid: " << book.get_best_bid() 
                  << " | Best Ask: " << book.get_best_ask() << std::endl;
        std::cout << "==================================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Engine Runtime Failure Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}