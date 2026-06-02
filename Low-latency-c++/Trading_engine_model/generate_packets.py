import struct

packet_format = "<cIIic"

NUM_PACKETS = 5_000_000

print(f"Generating {NUM_PACKETS} packets...")

with open("mock_market_data.bin", "wb") as f:
    for i in range(NUM_PACKETS):
        side = b'B' if i % 2 == 0 else b'S'
        order_id = i + 1
        qty = 1 + (i % 20)
        price = 900 + (i % 201)
        packet_type = b'Q' # Quote packet
        
        binary_data = struct.pack(packet_format, packet_type, order_id, qty, price, side)
        f.write(binary_data)

print("Done! Generated 'mock_market_data.bin' (14,000,000 bytes).")