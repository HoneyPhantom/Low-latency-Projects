#include <iostream>
#include <chrono>
#include "#Token_bucket.hpp"
using namespace std;

int main() {
    using clock = chrono::steady_clock;

    auto start = clock::now();
    auto start_ns = (uint64_t)chrono::duration_cast<chrono::nanoseconds>(start.time_since_epoch()).count();

    TokenBucket bucket(100000, 10000, start_ns);

    const int RUN_MS = 2000;
    uint64_t allowed = 0, denied = 0;

    while (true) {
        auto now = clock::now();
        auto now_ns = (uint64_t)chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();

        for (int i = 0; i < 500; ++i) {
            if (bucket.allow(now_ns)) ++allowed;
            else ++denied;
        }

        auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= RUN_MS) break;
    }

    cout << "Allowed: " << allowed << "\n";
    cout << "Denied : " << denied << "\n";
    cout << "Allowed/sec approx: " << (allowed / 2.0) << "\n";
}
