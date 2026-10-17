import capstone
import re
import sys

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

def get_str(va):
    off = va + 0x4000
    if off < 0 or off >= len(data): return ''
    end = data.find(b'\x00', off)
    s = data[off:end]
    if all(32 <= b < 127 for b in s):
        return s.decode('ascii')
    return ''

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
start_va = 0x45cd80
start_off = start_va + 0x4000
chunk = data[start_off:start_off+1200]

with open('scratch/fs_disasm.txt', 'w', encoding='utf-8') as out:
    for insn in md.disasm(chunk, start_va):
        extra = ''
        m = re.search(r'\[rip \+ (0x[0-9a-f]+)\]', insn.op_str)
        if m:
            target_va = insn.address + insn.size + int(m.group(1), 16)
            s = get_str(target_va)
            if s and len(s) < 50:
                extra = f' // "{s}"'
            else:
                extra = f' // VA 0x{target_va:x}'
        out.write(f'{insn.address:#x}: {insn.mnemonic} {insn.op_str}{extra}\n')

print("Disassembled to scratch/fs_disasm.txt")
