#pragma once
#include <iostream>
#include <cstddef>
#include <atomic>
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