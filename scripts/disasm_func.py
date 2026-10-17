import capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
start_va = 0x13f5fc0
start_off = start_va + 0x4000
chunk = data[start_off:start_off+200]

for insn in md.disasm(chunk, start_va):
    print(f'{insn.address:#x}: {insn.mnemonic} {insn.op_str}')
