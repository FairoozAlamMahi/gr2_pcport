import capstone
import re

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

start_va = 0x44b8f0
count = 700
off = start_va + 0x4000
insns = list(md.disasm(data[off:off+count], start_va))
for insn in insns:
    extra = ''
    m = re.search(r'\[rip \+ (0x[0-9a-f]+)\]', insn.op_str)
    if m:
        target_va = insn.address + insn.size + int(m.group(1), 16)
        extra = f' // VA 0x{target_va:x}'
    print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}{extra}")
