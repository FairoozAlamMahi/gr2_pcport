import struct

with open('game/eboot.elf', 'rb') as f:
    data = f.read()

e_phoff = struct.unpack_from('<Q', data, 0x20)[0]
e_phnum = struct.unpack_from('<H', data, 0x38)[0]

dynlib_off = 0
dynamic_off = 0

for i in range(e_phnum):
    p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from('<IIQQQQQQ', data, e_phoff + i * 56)
    if p_type == 0x61000000:
        dynlib_off = p_offset
    elif p_type == 2:
        dynamic_off = p_offset

dynlib = data[dynlib_off:]

tags = {}
idx = dynamic_off
while True:
    d_tag, d_val = struct.unpack_from('<qQ', data, idx)
    idx += 16
    if d_tag == 0: break
    tags[d_tag] = d_val

jmprel_off = tags.get(0x61000029, 0)
pltrelsz = tags.get(0x6100002D, 0)
symtab_off = tags.get(0x61000039, 0)
strtab_off = tags.get(0x61000035, 0)

count = pltrelsz // 24
for i in range(count):
    r_offset, r_info, r_addend = struct.unpack_from('<QQq', dynlib, jmprel_off + i * 24)
    sym_idx = r_info >> 32
    st_name = struct.unpack_from('<I', dynlib, symtab_off + sym_idx * 24)[0]
    s_end = dynlib.find(b'\0', strtab_off + st_name)
    sym_name = dynlib[strtab_off + st_name : s_end].decode('ascii', errors='ignore')
    if abs(r_offset - 0x1705138) < 0x80 or 'EventFlag' in sym_name:
        print(f"Reloc {hex(r_offset)} -> {sym_name}")
