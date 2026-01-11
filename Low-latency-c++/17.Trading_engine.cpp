#include <iostream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdint>
using namespace std;

template<typename T , size_t N>
class MemoryPool {
private:
    alignas(T) unsigned char buffer[N * sizeof(T)];
    struct FreeNode {
        FreeNode* next;
    };
    FreeNode* free_head = nullptr;
public:
    MemoryPool() {
        for(int i = 0; i < N; ++i){
            void* p = buffer + i * sizeof(T);
            FreeNode* node = static_cast<FreeNode*>(p);
            node->next = free_head;
            free_head = node;
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    T* alloc_raw(){
        if(!free_head) return nullptr;
        FreeNode* n = free_head;
        free_head = free_head->next;
        return reinterpret_cast<T*>(n);
    }

    void dealloc_raw(T* p){
        FreeNode* n = reinterpret_cast<FreeNode*>(p);
        n->next = free_head;
        free_head = n;
    }

    template <class... Args>
    T* create(Args&&... args) {
        T* p = alloc_raw();
        if (!p) return nullptr;
        return new (p) T(std::forward<Args>(args)...);
    }

    void destroy(T* p) {
        if (!p) return;
        p->~T();
        dealloc_raw(p);
    }
};

struct Order {
    int id;
    int qty;
    int price;
    bool is_buy;
    Order* next;

    Order(int id_, int qty_, int price_, bool is_buy_) : id(id_), qty(qty_), price(price_), is_buy(is_buy_), next(nullptr) {}
};

struct Level {
    Order* head = nullptr;
    Order* tail = nullptr;

    inline void push(Order* o){
        o->next = nullptr;
        if(!tail) head = tail = o;
        else{
            tail->next = o;
            tail = o;
        }
    }

    inline Order* pop(){
        Order* x = head;
        head = head->next;
        if(empty()) tail = nullptr;
        return x;
    }

    inline Order* top(){
        return head;
    }

    inline bool empty(){
        return !head;
    }
};

static constexpr int MIN_PRICE = 900;
static constexpr int MAX_PRICE = 1100;
static constexpr int LEVELS = MAX_PRICE - MIN_PRICE + 1;

inline int idx(int price){ 
    return price - MIN_PRICE; 
}

class OrderBook {
private:
    Level bids[LEVELS];
    Level asks[LEVELS];
    int best_bid = -1;
    int best_ask = -1;
    MemoryPool<Order , 1000000> pool;
    int next_idx = 1;

    inline void fix_best_bid(){
        if(best_bid == -1) return;
        while(best_bid >= MIN_PRICE && bids[idx(best_bid)].empty()) --best_bid;
        if(best_bid < MIN_PRICE) best_bid = -1;
    }

    inline void fix_best_ask(){
        if(best_ask == -1) return;
        while(best_ask <= MAX_PRICE && asks[idx(best_ask)].empty()) ++best_ask;
        if(best_ask > MAX_PRICE) best_ask = -1;
    }

    inline void enqueue_bid(int price , int qty){
        Order* o = pool.create(next_idx++ , qty , price , true);
        if(!o) return;
        bids[idx(price)].push(o);
        if(price > best_bid) best_bid = price;
    }

    inline void enqueue_ask(int price , int qty){
        Order* o = pool.create(next_idx++ , qty , price , false);
        if(!o) return;
        asks[idx(price)].push(o);
        if(best_ask == -1 || price < best_ask) best_ask = price;
    }

    void handle_buy(int price , int qty){
        if(best_ask == -1) return enqueue_bid(price , qty);
        while(qty && best_ask != -1 && best_ask <= price){
            Level& lvl = asks[idx(best_ask)];
            while(qty && !lvl.empty()){
                Order* o = lvl.top();
                int traded = min(o->qty , qty);
                o->qty -= traded;
                qty -= traded;
                if(o->qty == 0){
                    Order* done = lvl.pop();
                    pool.destroy(done);
                }
            }
            if(lvl.empty()) fix_best_ask();
        }
        if(qty) enqueue_bid(price , qty);
    }

    void handle_sell(int price , int qty){
        if(best_bid == -1) return enqueue_ask(price , qty);
        while(qty && best_bid != -1 && best_bid >= price){
            Level& lvl = bids[idx(best_bid)];
            while(qty && !lvl.empty()){
                Order* o = lvl.top();
                int traded = min(o->qty , qty);
                o->qty -= traded;
                qty -= traded;
                if(o->qty == 0){
                    Order* done = lvl.pop();
                    pool.destroy(done);
                }
            }
            if(lvl.empty()) fix_best_bid();
        }
        if(qty) enqueue_ask(price , qty);
    }

public:
    inline int get_best_bid() const{ 
        return best_bid; 
    }
    inline int get_best_ask() const{ 
        return best_ask; 
    }

    void add_in_limit(bool is_buy , int price , int qty){
        if(price < MIN_PRICE || price > MAX_PRICE || qty <= 0) return;
        if(is_buy) handle_buy(price , qty);
        else handle_sell(price , qty);
    }
};

OrderBook book;

int main() {
    using clock = std::chrono::steady_clock;

    const int N = 500000;
    std::vector<long long> lat_ns;
    lat_ns.reserve(N);
    uint32_t rng = 1;
    auto fast_rng = [&](uint32_t &s) -> uint32_t {
        s = s * 1664525u + 1013904223u;
        return s;
    };
    for (int i = 0; i < N; ++i) {
        uint32_t r = fast_rng(rng);
        bool is_buy = (r & 1);
        int price = MIN_PRICE + (r % LEVELS);
        int qty = 1 + (r % 20);

        auto t0 = clock::now();
        book.add_in_limit(is_buy, price, qty);
        auto t1 = clock::now();

        long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        lat_ns.push_back(ns);
    }
    sort(lat_ns.begin(), lat_ns.end());
    auto pct = [&](double p) {
        std::size_t idx = (std::size_t)((p / 100.0) * (lat_ns.size() - 1));
        return lat_ns[idx];
    };
    long long mn = lat_ns.front();
    long long mx = lat_ns.back();
    long long p50 = pct(50);
    long long p90 = pct(90);
    long long p99 = pct(99);
    long long p999 = pct(99.9);

    std::cout << "Orders: " << N << "\n";
    std::cout << "Best Bid: " << book.get_best_bid() << " | Best Ask: " << book.get_best_ask() << "\n";
    std::cout << "Latency(ns):\n";
    std::cout << "  min   = " << mn << "\n";
    std::cout << "  p50   = " << p50 << "\n";
    std::cout << "  p90   = " << p90 << "\n";
    std::cout << "  p99   = " << p99 << "\n";
    std::cout << "  p99.9 = " << p999 << "\n";
    std::cout << "  max   = " << mx << "\n";
}


