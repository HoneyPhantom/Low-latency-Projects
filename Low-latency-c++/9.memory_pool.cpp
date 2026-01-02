#include <iostream>
#include <cstddef>
using namespace std;

template<typename T , size_t N>
class MemoryPool {
private:
    alignas(T) unsigned char buffer[N * sizeof(T)];
    T* free_list[N];
    size_t free_count = N;
public:
    MemoryPool(){
        for(size_t i = 0; i < N; ++i) free_list[i] = reinterpret_cast<T*>(buffer + i * sizeof(T));
    }

    T* allocate(){
        if(free_count == 0) return nullptr;
        return free_list[--free_count];
    }

    void deallocate(T* ptr){
        ptr->~T();
        free_list[free_count++] = ptr;
    }
};

int main(){
    struct Node {
        int x;
        int y;
    };

    MemoryPool<Node , 1000> pool;

    Node* p = pool.allocate();
    new (p) Node{10 , 15};

    cout << p->x << " " << p->y;

    pool.deallocate(p);
    return 0;
}