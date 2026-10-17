import capstone
import re

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

def disasm_block(start_va, count, label):
    print(f"\n=================== {label} (0x{start_va:x}) ===================")
    off = start_va + 0x4000
    for insn in md.disasm(data[off:off+count], start_va):
        extra = ''
        m = re.search(r'\[rip \+ (0x[0-9a-f]+)\]', insn.op_str)
        if m:
            target_va = insn.address + insn.size + int(m.group(1), 16)
            extra = f' // VA 0x{target_va:x}'
        print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}{extra}")

disasm_block(0x44bd60, 100, "Stage Check 1 (0x44bd6f)")
disasm_block(0x44bd95, 100, "Stage Check 2 (0x44bd9f)")
disasm_block(0x44c865, 100, "Stage Check 3 (0x44c874)")
disasm_block(0x44da00, 100, "Stage Check 4 (0x44da03)")
