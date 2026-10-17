import capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

for va in range(0x464700, 0x463000, -1):
    off = va + 0x4000
    if data[off:off+4] == b'\x55\x48\x89\xe5' or data[off:off+3] == b'\x55\x48\x89':
        print(f"Function prologue found at 0x{va:x}")
        break
