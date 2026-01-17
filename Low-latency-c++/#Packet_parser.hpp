#pragma once
#include <cstdint>
#include <cstddef>
using namespace std;

struct PacketView {
    uint8_t type;
    uint16_t len;
    uint8_t checkSum;
    const uint8_t* payload;
};

static inline uint8_t payload_checksum_Xor(const uint8_t* p , size_t n){
    uint8_t x = 0;
    for(size_t i = 0; i < n; ++i) x ^= p[i];
    return x;
}

class PacketParser {
public:
    static inline bool parse(const uint8_t* buffer , size_t size , PacketView& out){
        if(!buffer || size < 4) return false;
        uint8_t type = buffer[0];
        uint16_t len = (uint16_t)buffer[1] | ((uint16_t)buffer[2] << 8);
        uint8_t checkSum = buffer[3];
        if(size < len + 4) return false;
        const uint8_t* payload = buffer + 4;
        uint8_t calc = payload_checksum_Xor(payload , len);
        if(calc != checkSum) return false;
        out.type = type;
        out.len = len;
        out.checkSum = checkSum;
        out.payload = payload;
        return true;
    }
};