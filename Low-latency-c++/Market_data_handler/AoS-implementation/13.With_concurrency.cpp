#include <iostream>
#include <atomic>
#include <thread>

using namespace std;

enum Side : uint8_t { BUY = 0, SELL = 1 };

struct Tick {
    int price;
    int qty;
    Side type;
};

inline static uint32_t fast_rng(uint32_t& state) {
    return state = state * 1664525u + 1013904223u;
}

const int RING_SIZE = 4096;
const int BATCH_SIZE = 64;

struct alignas(64) SharedState {
    Tick ring_buffer[RING_SIZE];
    alignas(64) atomic<uint32_t> head{0};
    alignas(64) atomic<uint32_t> tail{0};
    alignas(64) atomic<bool> done{false};
};

static SharedState ss;

void generate_ticks(int n) {
    uint32_t s = 1;
    for (int i = 0; i < n; i += BATCH_SIZE) {
        int current_batch = min(BATCH_SIZE, n - i);
        
        while (ss.head.load(memory_order_acquire) - ss.tail.load(memory_order_acquire) > (RING_SIZE - BATCH_SIZE)) {
            this_thread::yield();
        }

        uint32_t h = ss.head.load(memory_order_relaxed);
        for (int j = 0; j < current_batch; ++j) {
            uint32_t idx = (h + j) & (RING_SIZE - 1);
            uint32_t r = fast_rng(s);
            ss.ring_buffer[idx].type = (r & 1) ? BUY : SELL;
            ss.ring_buffer[idx].price = 1000 + (r % 100);
            ss.ring_buffer[idx].qty = 1 + (r % 50);
        }
        ss.head.fetch_add(current_batch, memory_order_release);
    }
    ss.done.store(true, memory_order_release);
}

void update_book(int& best_bid, int& best_ask) {
    uint32_t local_tail = 0;
    while (true) {
        uint32_t h = ss.head.load(memory_order_acquire);
        
        if (local_tail < h) {
            while (local_tail < h) {
                uint32_t idx = local_tail & (RING_SIZE - 1);
                const Tick& t = ss.ring_buffer[idx];
                if (t.type == BUY) {
                    if (t.price > best_bid) best_bid = t.price;
                } 
                else {
                    if (t.price < best_ask) best_ask = t.price;
                }
                ++local_tail;
            }
            ss.tail.store(local_tail, memory_order_release);
        } 
        else {
            if (ss.done.load(memory_order_acquire)) break;
            this_thread::yield();
        }
    }
}

int main() {
    const int N = 1e6;
    int best_bid = INT_MIN;
    int best_ask = INT_MAX;

    thread t1(generate_ticks, N);
    thread t2(update_book, ref(best_bid), ref(best_ask));

    t1.join();
    t2.join();

    cout << "Best Bid = " << best_bid << endl;
    cout << "Best Ask = " << best_ask << endl;

    return 0;
}