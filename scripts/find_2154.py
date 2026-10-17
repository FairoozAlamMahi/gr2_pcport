import capstone
import re

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

start_va = 0x464000
end_va = 0x466000
off = start_va + 0x4000
chunk = data[off : end_va + 0x4000]

print(f"Searching for 0x2154 across 0x{start_va:x} - 0x{end_va:x}:")
for insn in md.disasm(chunk, start_va):
    if '0x2154' in insn.op_str:
        print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}")
