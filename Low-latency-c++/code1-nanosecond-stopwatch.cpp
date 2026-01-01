#include <iostream>
#include <chrono>
using namespace std;
int main() {
    using clock = chrono::steady_clock;

    auto start = clock::now();
    volatile long long sum = 0;
    for(int i = 0; i < 1000000; ++i) sum += i;
    auto end = clock::now();
    
    auto ns = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
    cout << "Elapsed time: " << ns << "ns" << endl;
    return 0;
}
/*








*/