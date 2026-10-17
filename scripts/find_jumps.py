import struct

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

# Scan for any jmp or call (rel32: E9 or E8, or conditional jumps 0F 80..8F) targeting 0x465800..0x465a00
for offset in range(0x4000, 0x1400000):
    va = offset - 0x4000
    b = data[offset]
    if b in [0xE8, 0xE9]:
        rel = struct.unpack_from('<i', data, offset + 1)[0]
        tgt = va + 5 + rel
        if 0x465800 <= tgt <= 0x465a00:
            print(f"{'CALL' if b==0xE8 else 'JMP'} from {hex(va)} -> {hex(tgt)}")
    elif b == 0x0F and 0x80 <= data[offset+1] <= 0x8F:
        rel = struct.unpack_from('<i', data, offset + 2)[0]
        tgt = va + 6 + rel
        if 0x465800 <= tgt <= 0x465a00:
            print(f"Jcc from {hex(va)} -> {hex(tgt)}")
