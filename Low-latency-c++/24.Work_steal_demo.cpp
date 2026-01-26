#include <iostream>
#include <atomic>
#include <chrono>
#include "#Work_steal.hpp"
using namespace std;

int main() {
    WorkStealingScheduler schedu;
    atomic<int> counter{0};

    const int TASKS = 100000;

    auto start = chrono::steady_clock::now();

    for (int i = 0; i < TASKS; ++i) {
        schedu.submit(0, [&counter]() {
            counter.fetch_add(1, memory_order_relaxed);
        });
    }

    while (counter.load() < TASKS) {
        this_thread::sleep_for(chrono::milliseconds(1));
    }

    auto end = chrono::steady_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << "Executed " << counter << " tasks in " << ms << " ms\n";
}