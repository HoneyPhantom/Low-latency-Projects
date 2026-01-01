#include <iostream>
using namespace std;

template <typename T, int N>

class CircularBuffer {
    T data[N];
    int head = 0;
    int tail = 0;
    bool full = false;

public:
    bool push(const T& value) {
        data[tail] = value;
        if (full) head = (head + 1) % N;

        tail = (tail + 1) % N;
        full = (tail == head);
        return true;
    }

    bool pop(T& value) {
        if (empty()) return false;

        value = data[head];
        full = false;
        head = (head + 1) % N;
        return true;
    }

    bool empty() const {
        return (!full && head == tail);
    }
};

int main(){
    CircularBuffer<int , 5> s;
    s.push(10);
    s.push(20);
}