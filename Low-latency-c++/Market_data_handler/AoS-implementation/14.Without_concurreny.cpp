#include <iostream>
using namespace std;

enum Side : uint8_t { BUY = 0 , SELL = 1 };

struct Tick {
    int* prices;
    int* quantities;
    Side* types;
};

inline static uint32_t fast_rng(uint32_t& state){
    return state = state * 1664525u + 1013904223u;
}

void generate_ticks(Tick ticks , int n){
    uint32_t s = 1;
    for(int i = 0; i < n; ++i){
        uint32_t r = fast_rng(s);
        ticks.types[i] = (r & 1) ? BUY : SELL;
        ticks.prices[i] = 1000 + (r % 100);
        ticks.quantities[i] = 1 + (r % 50);
    }
}

inline void update_book(const Side& t , const int& p, int& best_bid , int& best_ask){
    if(t == BUY){
        if(p > best_bid) best_bid = p;
    }
    else{
        if(p < best_ask) best_ask = p;
    }
}

int main() {
    const int N = 1e6;
    static int prices[N];
    static int quantities[N];
    static Side types[N];
    Tick ticks = {prices , quantities , types};
    generate_ticks(ticks , N);
    int best_bid = INT_MIN;
    int best_ask = INT_MAX;
    for(int i = 0; i < N; ++i) update_book(ticks.types[i] , ticks.prices[i] , best_bid , best_ask);
    
    std::cout << "Best Bid = " << best_bid << "\n";
    std::cout << "Best Ask = " << best_ask << "\n";
    return 0;
}