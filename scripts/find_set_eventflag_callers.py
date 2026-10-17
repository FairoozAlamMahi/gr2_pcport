import struct, capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

target = 0x13f5fa0 # sceKernelSetEventFlag

print(f"Finding all calls to sceKernelSetEventFlag (0x{target:x}):")

for off in range(0x4000, 0x1400000):
    if data[off] == 0xE8:
        rel32 = struct.unpack_from('<i', data, off + 1)[0]
        va = off - 0x4000
        if va + 5 + rel32 == target:
            # Found call to sceKernelSetEventFlag
            # Disassemble preceding 30 bytes
            pre_start = max(0x4000, off - 30)
            chunk = data[pre_start : off + 5]
            insns = list(md.disasm(chunk, pre_start - 0x4000))
            print(f"\n--- Caller at 0x{va:x} ---")
            for insn in insns[-8:]:
                print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}")
