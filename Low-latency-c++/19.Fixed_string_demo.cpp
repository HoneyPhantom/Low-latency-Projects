#include <iostream>
#include <chrono>
#include "#Fixed_string.hpp"
using namespace std;

int main() {
    using clock = std::chrono::steady_clock;

    FixedString<256> fs;
    fs.append("price=");
    fs.append_int(12345);
    fs.append(", qty=");
    fs.append_int(50);

    cout << fs.view_string() << "\n";

    const int N = 2000000;

    for (int i = 0; i < 50000; ++i) {
        fs.clear();
        fs.append("id=");
        fs.append_int(i);
    }

    auto start = clock::now();
    for (int i = 0; i < N; ++i) {
        fs.clear();
        fs.append("id=");
        fs.append_int(i);
        fs.append(", px=");
        fs.append_int(1000 + (i % 100));
        fs.append(", qty=");
        fs.append_int(i % 50);
    }
    auto end = clock::now();

    auto ns = chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    cout << "Built " << N << " strings in " << ns << " ns\n";
    cout << "Avg " << (double)ns / N << " ns/string\n";
}