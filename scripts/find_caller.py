import struct

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

# E8 rel32
# Find all calls into 0x465000 .. 0x466000
for offset in range(0x4000, 0x1400000):
    if data[offset] == 0xE8:
        rel = struct.unpack_from('<i', data, offset + 1)[0]
        va = offset - 0x4000
        target = va + 5 + rel
        if 0x465000 <= target <= 0x466000:
            print(f"Call from {hex(va)} -> {hex(target)}")
