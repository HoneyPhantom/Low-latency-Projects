#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std;

template<typename T>
class LatestValue {
private:
    alignas(64) T buffers[2];
    atomic<uint64_t> version{0};
public:
    void publish(const T& a){
        uint64_t ver = version.load(memory_order_relaxed);

        version.store(ver + 1 , memory_order_release);

        buffers[(ver + 1) & 1] = a;

        version.store(ver + 2 , memory_order_release);
    }
    void read(T& out){
        uint64_t v1, v2;
        do{
            v1 = version.load(memory_order_acquire);
            if(v1 & 1) continue;

            out = buffers[v1 & 1];
            
            v2 = version.load(memory_order_acquire);
        }while(v1 != v2);
    }
};

struct Price{
    double bid;
    double ask;
};

int main(){
    const int N = 1000;
    mutex cout_mutex;
    LatestValue<Price> cache;
    atomic<bool> run{false};

    thread producer([&](){
        Price p{100.0 , 100.5};
        while(run.load()){
            p.bid += 0.01;
            p.ask += 0.01;
            cache.publish(p);
            this_thread::sleep_for(chrono::microseconds(50));
        }
    });

    auto consumer = [&](int id){
        Price p;
        for(int i = 0; i < 100000; ++i){
            cache.read(p);
            if(p.ask < p.bid){
                std::cout << "Data corruption detected!\n";
            }
        }
        lock_guard<mutex> lock(cout_mutex);
        std::cout << "Consumer " << id << " done\n";
    };

    thread consumers[N];
    for(int i = 0; i < N; ++i){
        consumers[i] = thread(consumer , i);
    }
    for(int i = 0; i < N; ++i){
        consumers[i].join();
    }

    run.store(false);
    producer.join();
    return 0;
}
