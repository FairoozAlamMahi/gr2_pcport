import capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

for start_va in [0x44e4c0, 0xfc6880, 0xfc6900]:
    print(f"\n=================== Disasm around 0x{start_va:x} ===================")
    off = start_va + 0x4000
    for insn in md.disasm(data[off:off+100], start_va):
        print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}")
