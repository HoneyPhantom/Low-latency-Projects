#pragma once
#include <cstdint>
#include <algorithm>

class TokenBucket {
    static constexpr uint64_t SCALE = 1000000000ull;
    uint64_t rate_per_sec;
    uint64_t burst_tokens;
    uint64_t capacity_scaled;
    uint64_t tokens_scaled;
    uint64_t last_ns; 
public:
    TokenBucket(uint64_t rate_tokens_per_sec, uint64_t burst, uint64_t start_ns)
        : rate_per_sec(rate_tokens_per_sec),
          burst_tokens(burst),
          capacity_scaled(burst * SCALE),
          tokens_scaled(burst * SCALE),
          last_ns(start_ns) {}

    inline bool allow(uint64_t now_ns) {
        refill(now_ns);
        if (tokens_scaled >= SCALE) {
            tokens_scaled -= SCALE;
            return true;
        }
        return false;
    }
    inline uint64_t tokens_available_scaled(uint64_t now_ns) {
        refill(now_ns);
        return tokens_scaled;
    }
private:
    inline void refill(uint64_t now_ns) {
        if (now_ns <= last_ns) return;
        uint64_t dt = now_ns - last_ns;
        last_ns = now_ns;
        __uint128_t add = (__uint128_t)dt * (__uint128_t)rate_per_sec;
        __uint128_t new_tokens = (__uint128_t)tokens_scaled + add;
        if (new_tokens > capacity_scaled) new_tokens = capacity_scaled;
        tokens_scaled = (uint64_t)new_tokens;
    }
};
