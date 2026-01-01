#include <iostream>
using namespace std;

int main(){
    using clock = chrono::steady_clock;
    const int N = 1000;
    int arr[N][N];

    auto start2 = clock::now();
    volatile long long sum2 = 0;
    for(int j = 0; j < N; ++j){
        for(int i = 0; i < N; ++i) sum2 += arr[i][j];
    }
    auto end2 = clock::now();

    auto ns2 = chrono::duration_cast<chrono::nanoseconds>(end2 - start2).count();

    cout << "Col Major traversal: " << ns2 << " ns" << endl;
    return 0;
}
/*




*/