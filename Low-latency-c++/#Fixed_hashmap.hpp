#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
using namespace std;

template <typename K>
struct DefaultHasher {
    static inline uint64_t hash(const K& k){
        const uint8_t* p = (const uint8_t*)&k;
        uint64_t h = 1469598103934665603ull;
        for (std::size_t i = 0; i < sizeof(K); ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
        return h;
    }
};

template<typename K , typename V , size_t N , typename Hasher = DefaultHasher<K>>
class FixedHashMap {
private:
    static_assert(N >= 8, "N too small");
    static_assert((N & (N - 1)) == 0, "N must be power of 2 for fast masking");
    static_assert(std::is_trivially_copyable<K>::value, "K should be trivially copyable");
    static_assert(std::is_trivially_copyable<V>::value, "V should be trivially copyable");

    enum State : uint8_t { EMPTY = 0, FILLED = 1, DELETED = 2 };

    struct Slot{
        K key;
        V value;
        State state;
    };

    Slot table[N];
    size_t filled = 0;

    static inline size_t mask() {
        return N - 1;
    }

public:
    FixedHashMap() {
        for(size_t i = 0; i < N; ++i) table[i].state = EMPTY;
    }

    inline size_t size() const{ return filled; }
    inline constexpr size_t capacity() const{ return N; };

    bool put(const K& key , const V& value){
        if((filled + 1) * 10 >= N * 7) return false;

        uint64_t h = Hasher::hash(key);
        size_t i = (size_t)h & mask();
        size_t first_tomb = -1;

        for(size_t probe = 0; probe < N; ++probe){
            Slot& slot = table[i];

            if(slot.state == EMPTY){
                size_t target = (first_tomb != -1 ? first_tomb : i);
                table[i].key = key;
                table[i].value = value;
                table[i].state = FILLED;
                ++filled;
                return true;
            }
            else if(slot.state == DELETED){
                if(first_tomb == -1) first_tomb = i;
            }
            else if(slot.key == key){
                slot.value = value;
                return true;
            }
            i = (i + 1) & mask();
        }
        return false;
    }

    bool get(const K& key , V& out) const{
        uint64_t h = Hasher::hash(key);
        size_t i = (size_t)h & mask();

        for(size_t probe = 0; probe < N; ++probe){
            const Slot& s = table[i];
            if(s.state == EMPTY) return false;
            if(s.state == FILLED && s.key == key){
                out = s.value;
                return true;
            }
            i = (i + 1) & mask();
        }
        return false;
    }

    bool erase(const K& key) {
        uint64_t h = Hasher::hash(key);
        size_t i = (size_t)h & mask();

        for (size_t probe = 0; probe < N; ++probe) {
            Slot& s = table[i];

            if (s.state == EMPTY) return false;
            if (s.state == FILLED && s.key == key){
                s.state = DELETED;
                --filled;
                return true;
            }

            i = (i + 1) & mask();
        }
        return false;
    }
};