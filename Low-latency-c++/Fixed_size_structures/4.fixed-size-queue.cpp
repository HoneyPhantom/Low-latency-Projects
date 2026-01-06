#include <iostream>
using namespace std;

template<typename T , int N>

class StaticQueue {
private:
    T data[N];
    int front_idx = 0;
    int back_idx = 0;
    int size = 0;
public:
    bool push(const T& value){
        if(size == N) return false;
        data[back_idx] = value;
        back_idx = (back_idx + 1) % N;
        ++size;
        return true;
    }

    bool pop(){
        if(empty()) return false;
        front_idx = (front_idx + 1) % N;
        --size;
        return true;
    }
    
    T& front(){
        return data[front_idx];
    }
    
    bool empty(){
        return size == 0;
    }
};

int main(){
    StaticQueue<int , 5> s;
    s.push(10);
    s.push(20);
    cout << s.front();
}