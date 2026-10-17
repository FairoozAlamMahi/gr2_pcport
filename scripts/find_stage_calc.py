import capstone
import re

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

start_va = 0x465600
count = 850
off = start_va + 0x4000

print(f"Disassembling 0x{start_va:x} to 0x{start_va+count:x}:")
for insn in md.disasm(data[off:off+count], start_va):
    if '0x2154' in insn.op_str or insn.address >= 0x465920:
        extra = ''
        m = re.search(r'\[rip \+ (0x[0-9a-f]+)\]', insn.op_str)
        if m:
            target_va = insn.address + insn.size + int(m.group(1), 16)
            extra = f' // VA 0x{target_va:x}'
        print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}{extra}")
