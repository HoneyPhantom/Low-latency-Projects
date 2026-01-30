#include <iostream>
#include <cstddef>
#include <atomic>
#include <thread>
#include "#SPSC_Ring_buffer.hpp"
using namespace std;

int main(){
    SPSCQueue<int , 1024> q;
    atomic<bool> run{true};

    thread producer([&](){
        int x = 0;
        while(run.load()) q.push(x++);
    });

    thread consumer([&](){
        int value;
        while(run.load()){
            if(q.pop(value)){
                cout << value << endl;
            }
        }
    });

    this_thread::sleep_for(chrono::microseconds(100));
    run.store(false);
    producer.join();
    consumer.join();
    return 0;
}
