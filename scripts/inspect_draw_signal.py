import capstone

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

# Disassemble from 0x46599d forwards
off = 0x46599d + 0x4000
chunk = data[off : off + 200]
print("Forward from 0x46599d:")
for insn in md.disasm(chunk, 0x46599d):
    print(f"{insn.address:#x}: {insn.mnemonic} {insn.op_str}")

# Now let's scan backwards for function starts (e.g. 55 48 89 e5 = push rbp; mov rbp, rsp)
for test_addr in range(0x46599d, 0x464000, -1):
    test_off = test_addr + 0x4000
    b = data[test_off:test_off+4]
    if b.startswith(b'\x55\x48\x89\xe5') or b.startswith(b'\x55\x41\x57') or b.startswith(b'\x55\x41\x56'):
        print(f"Candidate function start at: {hex(test_addr)}")
        # disassemble from test_addr to 0x46599d to verify instructions hit 0x46599d exactly
        cur = test_addr
        hit = False
        for insn in md.disasm(data[test_off:test_off+0x2000], test_addr):
            if insn.address == 0x46599d:
                hit = True
                break
            if insn.address > 0x46599d:
                break
        if hit:
            print(f"===> CONFIRMED function start: {hex(test_addr)}")
            break
