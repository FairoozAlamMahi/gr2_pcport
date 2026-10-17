import capstone
import re

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

start_va = 0x465f64
count = 350
off = start_va + 0x4000

for insn in md.disasm(data[off:off+count], start_va):
    extra = ''
    m = re.search(r'\[rip \+ (0x[0-9a-f]+)\]', insn.op_str)
    if m:
        target_va = insn.address + insn.size + int(m.group(1), 16)
        extra = f' // VA 0x{target_va:x}'
    print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}{extra}")
