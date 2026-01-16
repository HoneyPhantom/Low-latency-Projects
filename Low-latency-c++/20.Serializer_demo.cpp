#include <iostream>
#include <cstdint>
#include "#Byte_coder-decoder.hpp"
using namespace std;

struct TickMsg {
    uint64_t ts;
    int32_t price;
    int32_t qty;
    uint8_t side;
};

int main() {
    uint8_t buffer[64];

    TickMsg in { 1234567890123ull, 1012, 50, 1 };
    TickMsg out {};

    ByteWriter w(buffer, sizeof(buffer), Endian::Little);

    bool ok = true;
    ok &= w.write_u64(in.ts);
    ok &= w.write_i32(in.price);
    ok &= w.write_i32(in.qty);
    ok &= w.write_u8(in.side);

    if(!ok){
        std::cout << "Write failed\n";
        return 0;
    }

    ByteReader r(buffer, w.size(), Endian::Little);

    ok &= r.read_u64(out.ts);
    ok &= r.read_i32(out.price);
    ok &= r.read_i32(out.qty);
    ok &= r.read_u8(out.side);

    if(!ok){
        cout << "Read failed\n";
        return 0;
    }

    cout << "Decoded:\n";
    cout << "ts    =" << out.ts << endl;
    cout << "price =" << out.price  << endl;
    cout << "qty   =" << out.qty << endl;
    cout << "side  =" << (int)out.side << "\n";

    return 0;
}