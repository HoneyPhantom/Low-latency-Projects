#include <iostream>
#include <chrono>
using namespace std;

int main(){
    using clock = chrono::steady_clock;
    const int N = 1000;
    int arr[N][N];

    auto start1 = clock::now();
    volatile long long sum1 = 0;
    for(int i = 0; i < N; ++i){
        for(int j = 0; j < N; ++j) sum1 += arr[i][j];
    }
    auto end1 = clock::now();

    auto ns1 = chrono::duration_cast<chrono::nanoseconds>(end1 - start1).count();

    cout << "Row Major traversal: " << ns1 << " ns" << endl;
    return 0;
}
/*




*/