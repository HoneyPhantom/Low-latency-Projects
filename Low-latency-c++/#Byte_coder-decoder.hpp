#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
using namespace std;

enum class Endian : uint8_t { Little = 0, Big = 1 };

static inline uint16_t bswap16(uint16_t x){
    return (uint16_t)((x << 8) | (x >> 8));
}
static inline uint32_t bswap32(uint32_t x){
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}
static inline uint64_t bswap64(uint64_t x){
    return ((x & 0x00000000000000FFull) << 56) |
           ((x & 0x000000000000FF00ull) << 40) |
           ((x & 0x0000000000FF0000ull) << 24) |
           ((x & 0x00000000FF000000ull) << 8)  |
           ((x & 0x000000FF00000000ull) >> 8)  |
           ((x & 0x0000FF0000000000ull) >> 24) |
           ((x & 0x00FF000000000000ull) >> 40) |
           ((x & 0xFF00000000000000ull) >> 56);
}

static inline bool host_is_little_endian(){
    uint32_t x = 1;
    return *(reinterpret_cast<uint8_t*>(&x)) == 1;
}

template<typename T>
static inline T endian_convert(T v , Endian target){
    bool little = host_is_little_endian();
    if((little && target == Endian::Little) || (!little && target == Endian::Big)) return v;
    if (sizeof(T) == 2) return (T)bswap16((uint16_t)v);
    if (sizeof(T) == 4) return (T)bswap32((uint32_t)v);
    if (sizeof(T) == 8) return (T)bswap64((uint64_t)v);
    return v;
}


class ByteWriter {
private:
    uint8_t* buffer;
    size_t cap;
    size_t pos;
    Endian endian;
public:
    ByteWriter(void* buf , size_t capacity , Endian e = Endian::Little) : buffer((uint8_t*)buf) , cap(capacity) , pos(0) , endian(e) {}

    inline size_t size() const{
        return pos;
    }
    
    inline size_t remaining() const{
        return cap - pos;
    }

    inline const uint8_t* data() const{
        return buffer;
    }

    inline bool write_bytes(const void* scr , size_t n){
        if(pos + n > cap) return false;
        memcpy(buffer + pos, scr , n);
        pos += n;
        return true;
    }

    inline bool write_u8(uint8_t x){
        return write_bytes(&x , 1);
    }

    inline bool write_u16(uint16_t x){
        x = endian_convert<uint16_t>(x , endian);
        return write_bytes(&x , 2);
    }

    inline bool write_u32(uint32_t x){
        x = endian_convert<uint32_t>(x , endian);
        return write_bytes(&x , 4);
    }

    inline bool write_u64(uint64_t x){
        x = endian_convert<uint64_t>(x , endian);
        return write_bytes(&x , 8);
    }

    inline bool write_i32(int32_t x){
        return write_u32(x);
    }

    inline bool write_i64(int64_t x){
        return write_u64(x);
    }

    inline bool write_f64(double d){
        uint64_t x;
        memcpy(&x , &d , 8);
        x = endian_convert<uint64_t>(x , endian);
        return write_bytes(&x , 8);
    }
};

class ByteReader {
private:
    const uint8_t* buffer;
    size_t cap;
    size_t pos;
    Endian endian;
public:
    ByteReader(const void* scr , size_t size , Endian e = Endian::Little) : buffer((const uint8_t*)scr) , cap(size) , pos(0) , endian(e) {}

    inline size_t remaining() const{
        return cap - pos;
    }

    inline bool read_bytes(void* buf , size_t n){
        if(pos + n > cap) return false;
        memcpy(buf, buffer + pos , n);
        pos += n;
        return true;
    }

    inline bool read_u8(uint8_t& x){
        return read_bytes(&x , 1);
    }

    inline bool read_u16(uint16_t& x){
        if(!read_bytes(&x , 2)) return false;
        x = endian_convert<uint16_t>(x , endian);
        return true;
    }

    inline bool read_u32(uint32_t& x){
        if(!read_bytes(&x , 4)) return false;
        x = endian_convert<uint32_t>(x , endian);
        return true;
    }

    inline bool read_u64(uint64_t& x){
        if(!read_bytes(&x , 8)) return false;
        x = endian_convert<uint64_t>(x , endian);
        return true;
    }

    inline bool read_i32(int32_t& x){
        uint32_t u;
        if(!read_u32(u)) return false;
        x = u;
        return true;
    }

    inline bool read_i64(int64_t& x){
        uint64_t u;
        if(!read_u64(u)) return false;
        x = u;
        return true;
    }

    inline bool read_f64(double& d){
        uint64_t x;
        if(!read_bytes(&x , 8)) return false;
        x = endian_convert<uint64_t>(x , endian);
        memcpy(&d , &x , 8);
        return true;
    }
};