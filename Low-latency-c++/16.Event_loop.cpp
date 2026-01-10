#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <functional>
using namespace std;

template<typename T, size_t N>
class SPSCQueue {
private:
    T buffer[N];
    atomic<size_t> head{0};
    atomic<size_t> tail{0};
public:
    bool push(const T& value){
        size_t t = tail.load(memory_order_relaxed);
        size_t next = (t + 1) % N;
        if(next == head.load(memory_order_acquire)) return false;
        buffer[t] = value;
        tail.store(next , memory_order_release);
        return true;
    }

    bool pop(T& value){
        size_t h = head.load(memory_order_relaxed);
        size_t next = (h + 1) % N;
        if(next == tail.load(memory_order_acquire)) return false;
        value = buffer[h];
        head.store(next , memory_order_release);
        return true;
    }
};

class EventLoop {
private:
    using clock = chrono::steady_clock;

    struct Timer {
        clock::time_point next;
        chrono::microseconds interval;
        function<void()> fn;
    };

    SPSCQueue<function<void()> , 1024> queue;

    vector<Timer> timers;
    atomic<bool> run{true};
public:
    void post(const function<void()> &fn){
        queue.push(fn);
    }

    void add_timer_us(int timegap , function<void()> fn){
        Timer t;
        t.interval = chrono::microseconds(timegap);
        t.next = clock::now() + t.interval;
        t.fn = std::move(fn);
        timers.push_back(std::move(t));
    }

    void stop(){
        run.store(false , memory_order_release);
    }

    void loop(){
        while(run.load(memory_order_acquire)){
            bool did_work = false;
            function<void()> job;
            while(queue.pop(job)){
                job();
                did_work = true;
            }
            auto now = clock::now();
            for(auto &t : timers){
                if(t.next >= now){
                    t.fn();
                    t.next += t.interval;
                    did_work = true;
                }
            }
            if(!did_work) this_thread::sleep_for(chrono::microseconds(50));
        }
    }
};

int main() {
    EventLoop jobs;

    jobs.add_timer_us(500000 , [](){
        cout << "Timer fired after 0.5s" << endl;
    });

    jobs.post([](){
        cout << "Immediate job executed" << endl;
    });

    thread t([&](){
        jobs.loop();
    });

    this_thread::sleep_for(chrono::seconds(1));
    jobs.stop();
    t.join();
}