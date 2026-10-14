import struct

registered = {}
with open('src/loader/nid_table.cpp', 'r') as f:
    for line in f:
        if 'RegisterSymbol(' in line and '"' in line:
            parts = line.split('"')
            if len(parts) >= 2:
                nid = parts[1]
                name = parts[3] if len(parts) >= 4 else ''
                registered[nid] = name

print('Total registered host NIDs:', len(registered))

with open('../gr2_packages/merged_game/sce_module/libc.elf', 'rb') as f:
    dynlib_base = 0xd1ec0
    f.seek(dynlib_base + 0x18)
    strtab = f.read(0x9f25)
    f.seek(dynlib_base + 0x9f40)
    syms_raw = f.read(0xeda8)
    f.seek(dynlib_base + 0x18ce8)
    jmprel_raw = f.read(0x780)
    count = len(jmprel_raw) // 24
    matched = 0
    unmatched = []
    for i in range(count):
        r_off, r_info, r_addend = struct.unpack('<QQq', jmprel_raw[i*24:(i+1)*24])
        sym_idx = r_info >> 32
        name_off = struct.unpack('<I', syms_raw[sym_idx*24:sym_idx*24+4])[0]
        sym_name = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
        prefix = sym_name.split('#')[0] if '#' in sym_name else sym_name
        if prefix in registered:
            matched += 1
        else:
            unmatched.append((i, prefix, sym_name))

print(f'Matched in libc.elf jmprel: {matched}/{count}')
print(f'Unmatched count: {len(unmatched)}')
for i, p, s in unmatched[:30]:
    print(f'  unmatched: {p} ({s})')
