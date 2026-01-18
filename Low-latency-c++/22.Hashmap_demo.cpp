#include <iostream>
#include "#Fixed_hashmap.hpp"
using namespace std;

int main() {
    FixedHashMap<int, int, 1024> mp;

    mp.put(10, 100);
    mp.put(20, 200);
    mp.put(30, 300);

    int v;
    cout << "get(20) => " << (mp.get(20, v) ? v : -1) << "\n";

    mp.erase(20);
    cout << "after erase get(20) => " << (mp.get(20, v) ? v : -1) << "\n";

    for (int i = 1; i <= 600; ++i) {
        if (!mp.put(i, i * 10)) {
            std::cout << "put failed at i=" << i << "\n";
            break;
        }
    }
    cout << "size=" << mp.size() << "\n";
}
