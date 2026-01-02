#include <iostream>
using namespace std;

long long fast_pow(long long base , long long exp){
    long long res = 1;
    while(exp){
        if(exp & 1) res *= base;
        base *= base;
        exp >>= 1;
    }
    return res;
}

long long fast_mod_add(long long a , long long b , long long mod){
    long long sum = a + b;
    if(sum >= mod) sum -= mod;
    return sum;
}

long long fast_mod_mul(long long a , long long b , long long mod){
    long long res = 0;
    while(b){
        if(b & 1) res = fast_mod_add(res , a , mod);
        a = fast_mod_add(a , a , mod);
        b >>= 1;
    }
    return res;
}

int popcount_manual(unsigned int x){
    int cnt = 0;
    while(x){
        x &= (x - 1);
        ++cnt;
    }
    return cnt;
}

int main(){
    constexpr long long mod = 1e9 + 7;
    long long a = 1000, b = 10;

    long long pow = fast_pow(a , b);
    long long add = fast_mod_add(a , b , mod);
    long long mul = fast_mod_mul(a , b , mod);
    long long popcount = popcount_manual(a);

    cout << "pow: " << pow << endl;
    cout << "add: " << add << endl;
    cout << "mul: " << mul << endl;
    cout << "popcount: " << popcount << endl;
}