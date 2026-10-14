import struct

with open('game/eboot.elf', 'rb') as f:
    ehdr = f.read(64)
    phoff = struct.unpack('<Q', ehdr[32:40])[0]
    phnum = struct.unpack('<H', ehdr[56:58])[0]
    f.seek(phoff)
    phdrs = f.read(phnum * 56)

dynlib_off = 0
dyn_off = 0
for i in range(phnum):
    ptype, pflags, poff, pvaddr, ppaddr, pfilesz, pmemsz, palign = struct.unpack('<IIQQQQQQ', phdrs[i*56:(i+1)*56])
    if ptype == 0x61000000:
        dynlib_off = poff
    elif ptype == 2:
        dyn_off = poff

print('dynlib_off:', hex(dynlib_off), 'dyn_off:', hex(dyn_off))

with open('src/loader/nid_table.cpp', 'r') as f_table:
    registered = set()
    for line in f_table:
        if 'RegisterSymbol(' in line and '"' in line:
            parts = line.split('"')
            if len(parts) >= 2:
                registered.add(parts[1])

print('Total host registered NIDs:', len(registered))

with open('game/eboot.elf', 'rb') as f:
    f.seek(dyn_off)
    symtab_off = 0
    jmprel_off = 0
    pltrelsz = 0
    while True:
        tag, val = struct.unpack('<qQ', f.read(16))
        if tag == 0: break
        if tag == 0x61000039: symtab_off = val
        elif tag == 0x61000029: jmprel_off = val
        elif tag == 0x6100002D: pltrelsz = val

    f.seek(dynlib_off + 0x18)
    strtab = f.read(0x40000)
    f.seek(dynlib_off + symtab_off)
    total_syms = (jmprel_off - symtab_off) // 24
    print('Total eboot symbols:', total_syms)
    matched = 0
    symbols = []
    for i in range(2, total_syms):
        name_off, info, other, shndx, val, size = struct.unpack('<IBBHQQ', f.read(24))
        if name_off != 0 and shndx == 0:
            sym_name = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
            prefix = sym_name.split('#')[0] if '#' in sym_name else sym_name
            symbols.append((prefix, sym_name))
            if prefix in registered:
                matched += 1

# Add exports from the 4 decrypted PRX modules
prx_files = [
    '../gr2_packages/merged_game/sce_module/libc.elf',
    '../gr2_packages/merged_game/sce_module/libSceFios2.elf',
    '../gr2_packages/merged_game/sce_module/libSceJobManager.elf',
    '../gr2_packages/merged_game/sce_module/libSceNpToolkit.elf'
]

import os

prx_exports = {}
for prx in prx_files:
    if not os.path.exists(prx):
        print(f"Warning: PRX not found: {prx}")
        continue
    with open(prx, 'rb') as f_prx:
        data = f_prx.read()
        ehdr = data[:64]
        phoff = struct.unpack('<Q', ehdr[32:40])[0]
        phnum = struct.unpack('<H', ehdr[56:58])[0]
        dynlib_off = 0
        dyn_off = 0
        for i in range(phnum):
            ptype, pflags, poff, pvaddr, ppaddr, pfilesz, pmemsz, palign = struct.unpack('<IIQQQQQQ', data[phoff+i*56:phoff+(i+1)*56])
            if ptype == 0x61000000: dynlib_off = poff
            elif ptype == 2: dyn_off = poff
        
        symtab_off = 0
        symtab_sz = 0
        strtab_off = 0
        pos = dyn_off
        while pos + 16 <= len(data):
            tag, val = struct.unpack('<qQ', data[pos:pos+16])
            pos += 16
            if tag == 0: break
            if tag == 0x61000039: symtab_off = val
            elif tag == 0x6100003F: symtab_sz = val
            elif tag == 0x61000035: strtab_off = val

        strtab = data[dynlib_off + (strtab_off if strtab_off != 0 else 0x18):]
        syms_count = symtab_sz // 24
        prx_name = prx.split('/')[-1]
        for i in range(syms_count):
            name_off, info, other, shndx, val, size = struct.unpack('<IBBHQQ', data[dynlib_off + symtab_off + i*24 : dynlib_off + symtab_off + (i+1)*24])
            if val != 0 and name_off != 0:
                sname = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
                pfx = sname.split('#')[0] if '#' in sname else sname
                prx_exports[pfx] = (prx_name, sname)

print(f'Total PRX exports harvested: {len(prx_exports)}')

total_resolved = 0
prx_resolved = 0
host_resolved = 0
unresolved = []

for p, s in symbols:
    if p in registered:
        host_resolved += 1
        total_resolved += 1
    elif p in prx_exports:
        prx_resolved += 1
        total_resolved += 1
    else:
        unresolved.append((p, s))

print(f'EBOOT SYMBOL COVERAGE SUMMARY:')
print(f'  Total EBOOT imports: {len(symbols)}')
print(f'  Resolved by Host Subsystems: {host_resolved}')
print(f'  Resolved by Game PRXs: {prx_resolved}')
print(f'  TOTAL RESOLVED: {total_resolved}/{len(symbols)} ({total_resolved*100.0/len(symbols):.1f}%)')
print(f'  Remaining Unresolved: {len(unresolved)}')
print(f'\nSample Unresolved imports:')
for p, s in unresolved[:20]:
    print(f'  {p} ({s})')

