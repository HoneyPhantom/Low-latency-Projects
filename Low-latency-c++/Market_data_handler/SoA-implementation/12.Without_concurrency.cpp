#include <iostream>
using namespace std;

enum Side : uint8_t { BUY = 0 , SELL = 1 };

struct Tick {
    int price;
    int qty;
    Side type;
};

inline static uint32_t fast_rng(uint32_t& state){
    return state = state * 1664525u + 1013904223u;
}

void generate_ticks(Tick* ticks , int n){
    uint32_t s = 1;
    for(int i = 0; i < n; ++i){
        uint32_t r = fast_rng(s);
        ticks[i].type = (r & 1) ? BUY : SELL;
        ticks[i].price = 1000 + (r % 100);
        ticks[i].qty = 1 + (r % 50);
    }
}

inline void update_book(const Tick& t , int& best_bid , int& best_ask){
    if(t.type == BUY){
        if(t.price > best_bid) best_bid = t.price;
    }
    else{
        if(t.price < best_ask) best_ask = t.price;
    }
}

int main() {
    const int N = 1e6;
    static Tick ticks[N];
    generate_ticks(ticks , N);
    int best_bid = INT_MIN;
    int best_ask = INT_MAX;
    for(int i = 0; i < N; ++i) update_book(ticks[i] , best_bid , best_ask);
    
    std::cout << "Best Bid = " << best_bid << "\n";
    std::cout << "Best Ask = " << best_ask << "\n";
    return 0;
}
