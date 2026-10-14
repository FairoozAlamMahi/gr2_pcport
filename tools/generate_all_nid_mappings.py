import os
import json
import struct

print("Loading ps4libdoc database...")
nid_to_symbol = {}
for root, dirs, files in os.walk('tools/ps4libdoc'):
    for f in files:
        if f.endswith('.json'):
            try:
                with open(os.path.join(root, f), 'r', encoding='utf-8-sig', errors='ignore') as jf:
                    data = json.load(jf)
                    for mod in data.get('modules', []):
                        for lib in mod.get('libraries', []):
                            lib_name = lib.get('name', '')
                            for sym in lib.get('symbols', []):
                                enc = sym.get('encoded_id')
                                name = sym.get('name')
                                if enc and name:
                                    # Prefer libkernel or system libraries if collision
                                    if enc not in nid_to_symbol or 'libkernel' in f:
                                        nid_to_symbol[enc] = (f, lib_name, name)
            except:
                pass

print(f"Loaded {len(nid_to_symbol)} unique NIDs.")

def parse_elf_imports_and_exports(path):
    with open(path, 'rb') as f:
        data = f.read()
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
    jmprel_off = 0
    pltrelsz = 0
    pos = dyn_off
    while pos + 16 <= len(data):
        tag, val = struct.unpack('<qQ', data[pos:pos+16])
        pos += 16
        if tag == 0: break
        if tag == 0x61000039: symtab_off = val
        elif tag == 0x6100003F: symtab_sz = val
        elif tag == 0x61000035: strtab_off = val
        elif tag == 0x61000029: jmprel_off = val
        elif tag == 0x6100002D: pltrelsz = val

    strtab = data[dynlib_off + (strtab_off if strtab_off != 0 else 0x18):]
    
    # Imports from jmprel
    imports = []
    if jmprel_off and pltrelsz:
        count = pltrelsz // 24
        jmprel_raw = data[dynlib_off + jmprel_off : dynlib_off + jmprel_off + pltrelsz]
        for i in range(count):
            r_off, r_info, r_addend = struct.unpack('<QQq', jmprel_raw[i*24:(i+1)*24])
            sym_idx = r_info >> 32
            sym_entry = data[dynlib_off + symtab_off + sym_idx*24 : dynlib_off + symtab_off + (sym_idx+1)*24]
            name_off = struct.unpack('<I', sym_entry[:4])[0]
            sym_name = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
            prefix = sym_name.split('#')[0] if '#' in sym_name else sym_name
            imports.append((prefix, sym_name))

    # Exports
    exports = {}
    if symtab_off and symtab_sz:
        syms_count = symtab_sz // 24
        for i in range(syms_count):
            entry = data[dynlib_off + symtab_off + i*24 : dynlib_off + symtab_off + (i+1)*24]
            name_off, info, other, shndx, val, size = struct.unpack('<IBBHQQ', entry)
            if val != 0 and name_off != 0:
                sname = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
                pfx = sname.split('#')[0] if '#' in sname else sname
                exports[pfx] = sname

    return imports, exports

modules = {
    'eboot.elf': 'game/eboot.elf',
    'libc.elf': '../gr2_packages/merged_game/sce_module/libc.elf',
    'libSceFios2.elf': '../gr2_packages/merged_game/sce_module/libSceFios2.elf',
    'libSceJobManager.elf': '../gr2_packages/merged_game/sce_module/libSceJobManager.elf',
    'libSceNpToolkit.elf': '../gr2_packages/merged_game/sce_module/libSceNpToolkit.elf'
}

all_exports = {}
all_imports = {}

for name, path in modules.items():
    if os.path.exists(path):
        imp, exp = parse_elf_imports_and_exports(path)
        all_imports[name] = imp
        for pfx, full in exp.items():
            all_exports[pfx] = (name, full)
        print(f"[{name}] {len(imp)} imports, {len(exp)} exports")

# Now check all imports across all modules
host_required = {} # pfx -> (list of modules requiring it, (json_file, lib_name, real_name))
inter_module = {} # pfx -> (list of modules requiring it, (provider_mod, full_name))
still_unknown = {}

for mod_name, imps in all_imports.items():
    for pfx, full in imps:
        if pfx in all_exports:
            prov_mod, prov_full = all_exports[pfx]
            inter_module.setdefault(pfx, ([], prov_mod, prov_full))[0].append(mod_name)
        elif pfx in nid_to_symbol:
            info = nid_to_symbol[pfx]
            host_required.setdefault(pfx, ([], info))[0].append(mod_name)
        else:
            still_unknown.setdefault(pfx, []).append((mod_name, full))

print(f"\n================ SUMMARY ================")
print(f"Inter-module PRX links: {len(inter_module)} distinct functions")
print(f"Host System APIs required: {len(host_required)} distinct functions")
print(f"Still completely unknown: {len(still_unknown)} functions")

# Breakdown of Host APIs required by system library:
host_by_lib = {}
for pfx, (callers, (json_file, lib_name, real_name)) in host_required.items():
    host_by_lib.setdefault(json_file, []).append((pfx, real_name, callers))

print("\n--- HOST REQUIRED BY PS4 SYSTEM LIBRARY ---")
for json_file, syms in sorted(host_by_lib.items(), key=lambda x: -len(x[1])):
    print(f"  {json_file:45s}: {len(syms):3d} functions")

# Write out full C++ code ready for nid_table.cpp
with open("tools/generated_host_nids.cpp", "w", encoding="utf-8") as out:
    out.write("// Auto-generated Host NID Registration Table\n\n")
    for json_file, syms in sorted(host_by_lib.items(), key=lambda x: -len(x[1])):
        out.write(f"\n// ===== {json_file} ({len(syms)} symbols) =====\n")
        for pfx, real_name, callers in sorted(syms, key=lambda x: x[1]):
            callers_str = ", ".join(set(callers))
            out.write(f'RegisterSymbol("{pfx}", reinterpret_cast<uintptr_t>(&Kernel::{real_name}), "{real_name}"); // used by: {callers_str}\n')

print("\nWrote tools/generated_host_nids.cpp!")
