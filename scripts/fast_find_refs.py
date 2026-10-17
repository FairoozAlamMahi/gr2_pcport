import numpy as np
import capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

# Load as uint8 array
text_start = 0x4000
text_end = 0x1400000
text_len = text_end - text_start

raw = np.frombuffer(data, dtype=np.uint8, count=text_len, offset=text_start)

# We want 32-bit signed ints at offsets 0, 1, 2, 3 relative to each byte
# We can do 4 views at offsets 0, 1, 2, 3
targets = {
    0x1b30248: "FS_PreStep",
    0x1b30260: "FS_PostStep",
    0x1b30278: "FS_PreDraw",
    0x1b30290: "FS_PostDraw"
}

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

for target, name in targets.items():
    print(f"\n=== References to {name} (0x{target:x}) ===")
    matches = []
    for shift in range(4):
        # Slice raw from shift
        sub = raw[shift:]
        num_dwords = len(sub) // 4
        dwords = np.frombuffer(sub[:num_dwords*4].tobytes(), dtype=np.int32)
        
        # P = text_start + shift + np.arange(num_dwords) * 4
        # rip = P + 4 - 0x4000
        # target_calc = rip + dwords
        indices = np.arange(num_dwords, dtype=np.int64)
        rips = (shift + indices * 4 + 4) # relative to text_start VA
        computed_targets = rips + dwords
        
        hit_indices = np.where(computed_targets == target)[0]
        for idx in hit_indices:
            p_file = text_start + shift + int(idx) * 4
            matches.append(p_file)

    matches.sort()
    for p_file in matches:
        # Disassemble around p_file to see instruction
        # Instruction ends at p_file + 4
        insn_end_va = p_file + 4 - 0x4000
        # Check backwards up to 15 bytes
        chunk_start = max(0x4000, p_file - 15)
        chunk = data[chunk_start : p_file + 8]
        found = False
        for start_offset in range(chunk_start, p_file + 1):
            va = start_offset - 0x4000
            for insn in md.disasm(data[start_offset:p_file+4], va):
                if insn.address + insn.size == insn_end_va:
                    print(f"  {insn.address:#x}: {insn.mnemonic} {insn.op_str}")
                    found = True
                    break
            if found:
                break
        if not found:
            print(f"  [raw displacement at file {hex(p_file)} -> ends at VA {hex(insn_end_va)}]")
