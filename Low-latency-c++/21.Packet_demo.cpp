#include <iostream>
#include <cstring>
#include "#Packet_parser.hpp"
using namespace std;

static size_t build_packet(uint8_t type , uint16_t len , const uint8_t* payload , uint8_t* buffer , size_t cap){
    if(cap < len + 4) return 0;
    buffer[0] = type;
    buffer[1] = (uint8_t)(len & 0xFF);
    buffer[2] = (uint8_t)((len >> 8) & 0xFF);
    buffer[3] = payload_checksum_Xor(payload , len);
    memcpy(buffer + 4, payload , len);
    return 4 + len;
}

int main() {
    uint8_t payload[] = {10, 20, 30, 40, 50};
    uint8_t buf[64];

    std::size_t sz = build_packet(7, (uint16_t)sizeof(payload), payload, buf, sizeof(buf));
    cout << "Built packet size: " << sz << "\n";

    PacketView view{};
    bool ok = PacketParser::parse(buf, sz, view);

    cout << "Parse: " << (ok ? "OK" : "FAIL") << "\n";
    if (ok) {
        cout << "type     =" << (int)view.type << "\n";
        cout << "len      =" << view.len << "\n";
        cout << "checkSum =" << (int)view.checkSum << "\n";
        cout << "payload: \n";
        for (int i = 0; i < view.len; ++i) cout << (int)view.payload[i] << "\n";
        cout << "\n";
    }

    buf[4] ^= 0xFF;
    ok = PacketParser::parse(buf, sz, view);
    cout << "Parse after corruption: " << (ok ? "OK" : "FAIL") << "\n";
}