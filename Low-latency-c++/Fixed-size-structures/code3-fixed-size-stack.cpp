#include <iostream>
using namespace std;

template<typename T , int N>

class StaticStack {
private:
    T data[N];
    int curr_idx = -1;
public:
    bool push(const T& value){
        if(curr_idx + 1 == N) return false;
        data[++curr_idx] = value;
        return true;
    }

    bool pop(){
        if(empty()) return false;
        --curr_idx;
        return true;
    }
    
    T& top(){
        return data[curr_idx];
    }
    
    bool empty(){
        return curr_idx == -1;
    }
};

int main(){
    StaticStack<int , 5> s;
    s.push(10);
    s.push(20);
    cout << s.top();
}