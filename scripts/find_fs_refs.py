import struct, capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

targets = [0x1b30248, 0x1b30260, 0x1b30278, 0x1b30290]
names = {
    0x1b30248: "FS_PreStep",
    0x1b30260: "FS_PostStep",
    0x1b30278: "FS_PreDraw",
    0x1b30290: "FS_PostDraw"
}

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

for target in targets:
    print(f"\n--- References to {names[target]} (0x{target:x}) ---")
    for off in range(0x4000, 0x1400000):
        va = off - 0x4000
        for l in (5, 6, 7):
            disp = target - (va + l)
            if -0x80000000 <= disp <= 0x7FFFFFFF:
                disp_bytes = struct.pack('<i', disp)
                if data[off+l-4:off+l] == disp_bytes:
                    chunk = data[off:off+l+4]
                    insns = list(md.disasm(chunk, va))
                    if insns and insns[0].address == va:
                        print(f'{insns[0].address:#x}: {insns[0].mnemonic} {insns[0].op_str}')
