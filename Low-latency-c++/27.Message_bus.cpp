#include <iostream>
#include <atomic>
#include <thread>
#include "#SPSC_Ring_buffer.hpp"
using namespace std;

template<typename Msg, size_t SIZE, size_t TOPICS>
class MessageBus{
private:
    SPSCQueue<Msg , SIZE> queues[TOPICS];
public:
    bool publish(size_t topic , const Msg& m){
        if(topic >= TOPICS) return false;
        return queues[topic].push(m);
    }
    bool poll(size_t topic , Msg& m){
        if(topic >= TOPICS) return false;
        return queues[topic].pop(m);
    }
};

struct TickMsg {
    uint64_t ts;
    int price;
    int qty;
};

int main(){
    constexpr int TOPICS = 8;
    constexpr int SIZE = 1024;

    MessageBus<TickMsg , SIZE , TOPICS> bus;
    atomic<bool> run{true};

    thread producer([&](){
        uint64_t ts = 0;
        while (run.load()) {
            TickMsg m{ts++, 1000 + (int)(ts % 100), 1 + (int)(ts % 10)};
            bus.publish(ts % TOPICS, m);
        }
    });

    thread consumer([&](){
        TickMsg m;
        uint64_t count = 0;
        for(int i = 0; count < 1000000; i = (i + 1) % TOPICS){
            if(bus.poll(i , m)) ++count;
        }
        std::cout << "Consumed " << count << " messages\n";
        run.store(false);
    });

    consumer.join();
    producer.join();
    return 0;
}