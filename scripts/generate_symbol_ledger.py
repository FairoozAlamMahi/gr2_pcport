import os
import struct
import json

def extract_symbols_and_constants(elf_path, output_json):
    print(f"[SymbolLedger] Analyzing {elf_path} ...")
    with open(elf_path, 'rb') as f:
        data = f.read()

    # 1. ELF Header
    f_size = len(data)
    e_entry = struct.unpack('<Q', data[0x18:0x20])[0]
    e_phoff = struct.unpack('<Q', data[0x20:0x28])[0]
    e_phnum = struct.unpack('<H', data[0x38:0x3a])[0]

    # 2. Program Headers
    segments = []
    dynlib_off = None
    dynlib_sz = None
    rx_vaddr = None
    rx_sz = None
    rodata_candidates = []

    for i in range(e_phnum):
        off = e_phoff + i * 56
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack('<IIQQQQQQ', data[off:off+56])
        segments.append({
            'index': i, 'type': p_type, 'flags': p_flags,
            'offset': p_offset, 'vaddr': p_vaddr,
            'filesz': p_filesz, 'memsz': p_memsz
        })
        if p_type == 0x61000000: # PT_SCE_DYNLIBDATA
            dynlib_off = p_offset
            dynlib_sz = p_filesz
        elif p_type == 1 and (p_flags & 1) and (p_flags & 4): # RX (code)
            rx_vaddr = p_vaddr
            rx_sz = p_filesz
        elif p_type == 0x61000010: # PT_SCE_RELRO (rodata)
            rodata_candidates.append((p_offset, p_vaddr, p_filesz))

    # 3. Search for 30 FPS Delta Time float constant: 1.0f / 30.0f = 0.033333335f (0x3D088889 in IEEE-754)
    # Also 60 FPS: 0.016666668f (0x3C888889)
    dt_30_bytes = struct.pack('<f', 1.0 / 30.0)
    dt_60_bytes = struct.pack('<f', 1.0 / 60.0)
    
    dt_30_hits = []
    pos = 0
    while len(dt_30_hits) < 50:
        idx = data.find(dt_30_bytes, pos)
        if idx == -1: break
        dt_30_hits.append(hex(idx))
        pos = idx + 4

    dt_60_hits = []
    pos = 0
    while len(dt_60_hits) < 50:
        idx = data.find(dt_60_bytes, pos)
        if idx == -1: break
        dt_60_hits.append(hex(idx))
        pos = idx + 4

    print(f"[SymbolLedger] Found {len(dt_30_hits)} occurrences of 30 FPS dt (0.033333f)")
    print(f"[SymbolLedger] Found {len(dt_60_hits)} occurrences of 60 FPS dt (0.016666f)")

    # 4. Extract imported libraries from PT_SCE_DYNLIBDATA
    imported_libs = []
    if dynlib_off:
        dyn_data = data[dynlib_off:dynlib_off+0x2000]
        # find null-terminated strings starting with 'lib'
        raw_strs = [s.decode('ascii', errors='ignore') for s in dyn_data.split(b'\x00') if len(s) > 2]
        for s in raw_strs:
            if ('lib' in s or 'Sce' in s) and s not in imported_libs:
                imported_libs.append(s)

    # 5. Extract engine strings
    engine_terms = [
        "LaunchScript", "root", "Gravite", "Gravity", "Kat", "Raven", "Syd",
        "Hekseville", "Jirga", "Boutoume", "DeltaTime", "deltaTime", "framerate"
    ]
    string_locations = {}
    for term in engine_terms:
        term_bytes = term.encode('ascii')
        term_hits = []
        pos = 0
        while len(term_hits) < 10:
            idx = data.find(term_bytes, pos)
            if idx == -1: break
            term_hits.append(hex(idx))
            pos = idx + len(term_bytes)
        if term_hits:
            string_locations[term] = term_hits

    # 6. Build Ledger
    ledger = {
        'game_title': 'Gravity Rush 2',
        'title_id': 'CUSA04943',
        'app_version': '01.11',
        'target_arch': 'AMD64 (x86-64)',
        'os_abi': 'FreeBSD / Orbis OS (ELF64)',
        'entry_point': hex(e_entry),
        'file_size': f_size,
        'segments': segments,
        'imported_libraries': imported_libs,
        'delta_time_30fps_constants': dt_30_hits[:20],
        'delta_time_60fps_constants': dt_60_hits[:20],
        'engine_symbol_locations': string_locations,
        'architectural_patches': {
            'calling_convention': 'System V AMD64 (__attribute__((sysv_abi)))',
            'tls_rewrite': 'mov reg, fs:[0] -> mov reg, gs:[0x1480 + 8*slot]',
            'memory_pool': 'PS4 Pro Neo Mode (+4096 MB DMEM -> 9.5 GB total)',
            'synthetic_touchpad': 'LB + D-Pad Up/Down (8-frame swipe trajectories)',
            'high_entropy_aslr': 'Disabled (/HIGHENTROPYVA:NO / --disable-high-entropy-va)'
        }
    }

    os.makedirs(os.path.dirname(output_json), exist_ok=True)
    with open(output_json, 'w') as out:
        json.dump(ledger, out, indent=2)

    print(f"[SymbolLedger] Ledger written to {output_json} successfully!")
    return ledger

if __name__ == "__main__":
    elf = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2port\game\eboot.elf"
    out = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2port\docs\gr2_symbol_ledger.json"
    extract_symbols_and_constants(elf, out)
