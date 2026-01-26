#pragma once
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
using namespace std;

class SpinLock {
private:
    atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(memory_order_acquire)) { }
    }
    void unlock() {
        flag.clear(memory_order_release);
    }
};

template<size_t N>
class WorkStealDeque {
private:
    function<void()> buffer[N];
    size_t head = 0;
    size_t tail = 0;
    SpinLock lock;
public:
    bool push(function<void()> job){
        lock.lock();
        if(tail - head >= N){
            lock.unlock();
            return false;
        }
        buffer[tail % N] = std::move(job);
        ++tail;
        lock.unlock();
        return true;
    }

    bool pop(function<void()>& job){
        lock.lock();
        if(head == tail){
            lock.unlock();
            return false;
        }
        --tail;
        job = std::move(buffer[tail % N]);
        lock.unlock();
        return true;
    }

    bool steal(function<void()>& job){
        lock.lock();
        if(head == tail){
            lock.unlock();
            return false;
        }
        job = std::move(buffer[head % N]);
        ++head;
        lock.unlock();
        return true;
    }
};

class WorkStealingScheduler {
private:
    static constexpr size_t SIZE = 1024;
    WorkStealDeque<SIZE> deques[2];
    atomic<bool> run{true};
    thread workers[2];

    void worker_loop(int id){
        int otherid = id ^ 1;
        function<void()> job;
        while(run.load(memory_order_acquire)){
            if(deques[id].pop(job) || deques[otherid].steal(job)) job();
            else this_thread::yield();
        }
    }
public:
    WorkStealingScheduler() {
        for(int i = 0; i < 2; ++i){
            workers[i] = thread([this, i]() { worker_loop(i); });
        }
    }
    void submit(int worker_id , function<void()> job){
        deques[worker_id & 1].push(std::move(job)); 
    }
    ~WorkStealingScheduler() {
        run.store(false , memory_order_release);
        for(auto& t : workers) t.join();
    }
};