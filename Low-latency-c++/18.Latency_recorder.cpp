#include <iostream>
#include <chrono>
#include "#Latency_recorder.hpp"
using namespace std;

inline void dummy_work(int iter) {
    volatile int x = 0;
    for(int i = 0; i < iter; ++i) x += i;
}

int main() {
    using clock = chrono::steady_clock;

    const int N = 200000;
    LatencyRecorder rec(N , 50 , 3000);

    for(int i = 0; i < 5000; ++i) dummy_work(50);
    
    for(int i = 0; i < N; ++i){
        auto start = clock::now();
        dummy_work(50);
        auto end = clock::now();
        uint64_t ns = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
        rec.add(ns);
    }

    rec.report(cout , 5000);
}