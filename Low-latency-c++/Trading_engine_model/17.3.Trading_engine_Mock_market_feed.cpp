#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>

#include <fstream>
#pragma pack(push, 1)
struct MarketDataPacket {
    char type;         // Offset 0
    uint32_t order_id; // Offset 1
    uint32_t qty;      // Offset 5
    int price;         // Offset 9
    char side;         // Offset 13
};
#pragma pack(pop)

using namespace std;

static constexpr uint32_t NULL_IDX = 0xFFFFFFFF;

template<typename T , size_t N>
class MemoryPool {
private:
    alignas(64) char buffer[N * sizeof(T)];
    struct FreeNode { uint32_t next_idx; };
    uint32_t free_head = 0;
    
    inline T* get_slot(size_t idx) {
        return reinterpret_cast<T*>(&buffer[idx * sizeof(T)]);
    }

    inline const T* get_slot(size_t idx) const {
        return reinterpret_cast<const T*>(&buffer[idx * sizeof(T)]);
    }
public:
    MemoryPool() {
        for(int i = 0; i < N - 1; ++i){
            FreeNode* node = reinterpret_cast<FreeNode*>(get_slot(i));
            node->next_idx = static_cast<uint32_t>(i + 1);
        }
        FreeNode* last_node = reinterpret_cast<FreeNode*>(get_slot(N - 1));
        last_node->next_idx = static_cast<uint32_t>(NULL_IDX);
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    uint32_t alloc_raw(){
        if(free_head == NULL_IDX) [[unlikely]] return NULL_IDX;
        uint32_t node = free_head;
        FreeNode* curr = reinterpret_cast<FreeNode*>(get_slot(node));
        free_head = curr->next_idx;
        return node;
    }

    void dealloc_raw(uint32_t idx){
        if(idx == NULL_IDX || idx >= N) return;
        FreeNode* node = reinterpret_cast<FreeNode*>(get_slot(idx));
        node->next_idx = free_head;
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
        if(idx == NULL_IDX || idx >= N) return;
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
        else{
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

inline int idx(int price){ 
    return price - MIN_PRICE; 
}

class Pricemap {
private:
    uint64_t masks[4] = {0};
public:
    inline void set(int idx) { 
        masks[idx >> 6] |= (1ULL << (idx & 63)); 
    } 

    inline void clear(int idx) {
        masks[idx >> 6] &= ~(1ULL << (idx & 63));
    }

    inline int find_max() const {
        for(int i = 3; i >= 0; --i){
            if(masks[i]){
                int bit_idx = 63 - __builtin_clzll(masks[i]);
                return (i << 6) | bit_idx;
            }
        }
        return -1;
    }

    inline int find_min() const {
        for(int i = 0; i < 4; ++i){
            if(masks[i]){
                int bit_idx = __builtin_ctzll(masks[i]);
                return (i << 6) | bit_idx;
            }
        }
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
            if (lvl.empty()) [[unlikely]] {
                ask_mask.clear(best_ask_idx);
                continue;
            }
            while(qty > 0 && !lvl.empty()){
                uint32_t id = lvl.top();
                Order& o = pool[id];
                uint32_t traded = min(o.qty , qty);
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
            if (lvl.empty()) [[unlikely]] {
                bid_mask.clear(best_bid_idx);
                continue;
            }
            while(qty > 0 && !lvl.empty()){
                uint32_t id = lvl.top();
                Order& o = pool[id];
                uint32_t traded = min(o.qty , qty);
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

int main() {
    // 1. Thread affinity core locking for Apple Silicon performance channels
    thread_affinity_policy_data_t policy = { 3 };
    thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);

    std::ifstream file("mock_market_data.bin", std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open mock_market_data.bin" << std::endl;
        return 0;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    if (!file.read(buffer.data(), file_size)) {
        std::cerr << "Error reading file data." << std::endl;
        return 0;
    }
    file.close();

    constexpr size_t packet_size = sizeof(MarketDataPacket);
    const size_t total_packets = file_size / packet_size;

    std::cout << "Parsing " << total_packets << " packets via pointer casting..." << std::endl;

    vector<long long> lat_ns;
    lat_ns.reserve(total_packets);
    
    using clock = std::chrono::steady_clock;

    auto start = clock::now();

    const char* curr = buffer.data();
    for (int i = 0; i < total_packets; ++i, curr += packet_size) {
        const auto& ord = reinterpret_cast<const MarketDataPacket*>(curr);
        book.add_in_limit(ord->side == 'B', ord->price, ord->qty);
    }

    auto end = clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Processed " << total_packets << " packets in " << duration << " microseconds." << "\n";
    std::cout << "Average time per packet: " << (double)duration * 1000.0 / total_packets << " nanoseconds." << "\n";

    return 0;
}