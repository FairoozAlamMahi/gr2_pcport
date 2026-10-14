import struct

elf_path = 'game/eboot.elf'
with open(elf_path, 'rb') as f:
    data = f.read()

ehdr = data[:64]
phoff = struct.unpack('<Q', ehdr[32:40])[0]
phnum = struct.unpack('<H', ehdr[56:58])[0]

dynlib_off = 0
dyn_off = 0
code_segs = []

for i in range(phnum):
    ph = data[phoff+i*56 : phoff+(i+1)*56]
    ptype, pflags, poff, pvaddr, ppaddr, pfilesz, pmemsz, palign = struct.unpack('<IIQQQQQQ', ph)
    if ptype == 0x61000000:
        dynlib_off = poff
    elif ptype == 2:
        dyn_off = poff
    elif ptype == 1 and (pflags & 1):
        code_segs.append((poff, pvaddr, pfilesz))

pos = dyn_off
jmprel_off = 0
pltrelsz = 0
symtab_off = 0
symtab_sz = 0
strtab_off = 0
strtab_sz = 0

while pos + 16 <= len(data):
    tag, val = struct.unpack('<qQ', data[pos:pos+16])
    pos += 16
    if tag == 0: break
    elif tag == 0x61000029: jmprel_off = val
    elif tag == 0x6100002D: pltrelsz = val
    elif tag == 0x61000039: symtab_off = val
    elif tag == 0x6100003F: symtab_sz = val
    elif tag == 0x61000035: strtab_off = val
    elif tag == 0x61000037: strtab_sz = val

dynlib = data[dynlib_off:]
strtab = dynlib[(strtab_off if strtab_off != 0 else 0x18):]

syms = []
sym_count = symtab_sz // 24 if symtab_sz != 0 else 0
for i in range(sym_count):
    soff = symtab_off + i * 24
    st_name, st_info, st_other, st_shndx, st_val, st_size = struct.unpack('<IBBHQQ', dynlib[soff:soff+24])
    sym_name = ""
    if st_name < len(strtab):
        sym_name = strtab[st_name:].split(b'\0')[0].decode('utf-8', errors='ignore')
    syms.append(sym_name)

target_nids = {"rPo6tV8D9bM": "sceSystemServiceGetStatus", "656LMQSrg6U": "sceSystemServiceReceiveEvent"}
found_relocs = {}

count = pltrelsz // 24
for i in range(count):
    roff = jmprel_off + i * 24
    r_offset, r_info, r_addend = struct.unpack('<QQq', dynlib[roff:roff+24])
    rtype = r_info & 0xFFFFFFFF
    sym_idx = r_info >> 32
    if sym_idx < len(syms):
        sname = syms[sym_idx]
        prefix = sname.split('#')[0] if '#' in sname else sname
        if prefix in target_nids:
            print(f"Found JMPREL for {prefix} ({target_nids[prefix]}): r_offset={hex(r_offset)}")
            found_relocs[r_offset] = (prefix, target_nids[prefix])

# Find PLT entries in first 0x20000 of code
for r_offset, (nid, fn_name) in found_relocs.items():
    for c_off, c_vaddr, c_sz in code_segs:
        code_bytes = data[c_off : c_off + min(c_sz, 0x40000)]
        idx = 0
        while True:
            idx = code_bytes.find(b'\xff\x25', idx)
            if idx == -1: break
            disp = struct.unpack('<i', code_bytes[idx+2 : idx+6])[0]
            if (c_vaddr + idx + 6 + disp) == r_offset:
                plt_va = c_vaddr + idx
                print(f"PLT entry for {fn_name}: {hex(plt_va)}")
            idx += 1
