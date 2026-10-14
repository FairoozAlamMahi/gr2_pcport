import os
import json
import struct

print("Indexing ps4libdoc...")
nid_to_symbol = {}
libdoc_root = "tools/ps4libdoc"

for root, dirs, files in os.walk(libdoc_root):
    for f in files:
        if f.endswith(".json"):
            filepath = os.path.join(root, f)
            try:
                with open(filepath, "r", encoding="utf-8-sig", errors="ignore") as jf:
                    data = json.load(jf)
                    if isinstance(data, dict):
                        modules = data.get("modules", [])
                        if isinstance(modules, list):
                            for mod in modules:
                                libraries = mod.get("libraries", [])
                                if isinstance(libraries, list):
                                    for lib in libraries:
                                        lib_name = lib.get("name", "")
                                        symbols = lib.get("symbols", [])
                                        if isinstance(symbols, list):
                                            for sym in symbols:
                                                enc_id = sym.get("encoded_id")
                                                name = sym.get("name")
                                                if enc_id and name:
                                                    nid_to_symbol[enc_id] = (f, lib_name, name)
            except Exception as e:
                pass

print(f"Indexed {len(nid_to_symbol)} unique encoded NIDs from ps4libdoc.")

# Parse game PRX exports
prx_files = [
    '../gr2_packages/merged_game/sce_module/libc.elf',
    '../gr2_packages/merged_game/sce_module/libSceFios2.elf',
    '../gr2_packages/merged_game/sce_module/libSceJobManager.elf',
    '../gr2_packages/merged_game/sce_module/libSceNpToolkit.elf'
]

prx_exports = {}
for prx in prx_files:
    if not os.path.exists(prx):
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
        prx_name = os.path.basename(prx)
        for i in range(syms_count):
            name_off, info, other, shndx, val, size = struct.unpack('<IBBHQQ', data[dynlib_off + symtab_off + i*24 : dynlib_off + symtab_off + (i+1)*24])
            if val != 0 and name_off != 0:
                sname = strtab[name_off:].split(b'\0')[0].decode('utf-8', errors='ignore')
                pfx = sname.split('#')[0] if '#' in sname else sname
                prx_exports[pfx] = (prx_name, sname)

print(f"Indexed {len(prx_exports)} PRX exports from decrypted game modules.")

# Parse eboot imports
with open("game/eboot.elf", "rb") as f:
    ehdr = f.read(64)
    phoff = struct.unpack("<Q", ehdr[32:40])[0]
    phnum = struct.unpack("<H", ehdr[56:58])[0]
    f.seek(phoff)
    phdrs = f.read(phnum * 56)

dynlib_off = 0
dyn_off = 0
for i in range(phnum):
    ptype, pflags, poff, pvaddr, ppaddr, pfilesz, pmemsz, palign = struct.unpack("<IIQQQQQQ", phdrs[i*56:(i+1)*56])
    if ptype == 0x61000000: dynlib_off = poff
    elif ptype == 2: dyn_off = poff

with open("game/eboot.elf", "rb") as f:
    f.seek(dyn_off)
    symtab_off = 0
    jmprel_off = 0
    while True:
        tag, val = struct.unpack("<qQ", f.read(16))
        if tag == 0: break
        if tag == 0x61000039: symtab_off = val
        elif tag == 0x61000029: jmprel_off = val

    f.seek(dynlib_off + 0x18)
    strtab = f.read(0x40000)
    f.seek(dynlib_off + symtab_off)
    total_syms = (jmprel_off - symtab_off) // 24
    
    eboot_imports = []
    for i in range(2, total_syms):
        name_off, info, other, shndx, val, size = struct.unpack("<IBBHQQ", f.read(24))
        if name_off != 0 and shndx == 0:
            sym_name = strtab[name_off:].split(b"\0")[0].decode("utf-8", errors="ignore")
            prefix = sym_name.split("#")[0] if "#" in sym_name else sym_name
            eboot_imports.append((prefix, sym_name))

print(f"Total eboot imports: {len(eboot_imports)}")

by_library = {}
prx_matched = []
unmatched = []

for prefix, full_sym in eboot_imports:
    if prefix in prx_exports:
        prx_matched.append((prefix, full_sym, prx_exports[prefix]))
    elif prefix in nid_to_symbol:
        json_file, lib_name, real_name = nid_to_symbol[prefix]
        by_library.setdefault(json_file, []).append((prefix, full_sym, real_name))
    else:
        unmatched.append((prefix, full_sym))

print(f"\n--- PRX RESOLVED: {len(prx_matched)} symbols ---")
print(f"--- PS4LIBDOC IDENTIFIED: {sum(len(v) for v in by_library.values())} symbols ---")
print(f"--- UNMATCHED: {len(unmatched)} symbols ---")

print("\n--- BREAKDOWN BY PS4 SYSTEM LIBRARY ---")
for lib_json, syms in sorted(by_library.items(), key=lambda x: -len(x[1])):
    print(f"{lib_json:45s}: {len(syms):3d} symbols")

# Output full detail to a text file for complete reference
with open("tools/eboot_symbol_analysis.txt", "w", encoding="utf-8") as out:
    out.write("=== EBOOT SYMBOL ANALYSIS ===\n\n")
    out.write(f"Total eboot imports: {len(eboot_imports)}\n")
    out.write(f"Resolved by PRX: {len(prx_matched)}\n")
    out.write(f"Resolved by PS4LIBDOC: {sum(len(v) for v in by_library.values())}\n")
    out.write(f"Unmatched: {len(unmatched)}\n\n")

    for lib_json, syms in sorted(by_library.items(), key=lambda x: -len(x[1])):
        out.write(f"\n### {lib_json} ({len(syms)} symbols)\n")
        for pfx, full, real in sorted(syms, key=lambda x: x[2]):
            out.write(f'  RegisterSymbol("{pfx}", reinterpret_cast<uintptr_t>(&{real}), "{real}"); // {full}\n')

    if unmatched:
        out.write("\n### UNMATCHED SYMBOLS\n")
        for pfx, full in unmatched:
            out.write(f"  {pfx}: {full}\n")

print("\nWrote tools/eboot_symbol_analysis.txt with full mapping!")
