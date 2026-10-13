import os
import sys
import struct
import json

def parse_elf(elf_path):
    with open(elf_path, 'rb') as f:
        # ELF Header
        magic = f.read(16)
        if magic[:4] != b'\x7fELF':
            raise ValueError("Not an ELF file")
            
        e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack('<HHIQQQIHHHHHH', f.read(48))
        
        print(f"Type: {e_type:#x}, Machine: {e_machine:#x}, Entry: {e_entry:#x}")
        print(f"Program Headers: {e_phnum} at offset {e_phoff:#x}")
        print(f"Section Headers: {e_shnum} at offset {e_shoff:#x}")
        
        # Read Program Headers
        f.seek(e_phoff)
        phs = []
        dynamic_ph = None
        for i in range(e_phnum):
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack('<IIQQQQQQ', f.read(56))
            ph_info = {
                'index': i, 'type': p_type, 'flags': p_flags,
                'offset': p_offset, 'vaddr': p_vaddr, 'paddr': p_paddr,
                'filesz': p_filesz, 'memsz': p_memsz, 'align': p_align
            }
            phs.append(ph_info)
            if p_type == 2: # PT_DYNAMIC
                dynamic_ph = ph_info
                
        print("\n--- Program Headers ---")
        for ph in phs:
            t_name = {1: "PT_LOAD", 2: "PT_DYNAMIC", 3: "PT_INTERP", 7: "PT_TLS", 0x61000000: "PT_SCE_DYNLIBDATA", 0x61000010: "PT_SCE_RELRO"}.get(ph['type'], f"0x{ph['type']:x}")
            flags_str = ""
            if ph['flags'] & 4: flags_str += "R"
            if ph['flags'] & 2: flags_str += "W"
            if ph['flags'] & 1: flags_str += "X"
            print(f"  [{ph['index']:02d}] {t_name:18s} Offset: 0x{ph['offset']:08x} VAddr: 0x{ph['vaddr']:08x} FileSz: 0x{ph['filesz']:08x} MemSz: 0x{ph['memsz']:08x} Flags: {flags_str}")

        # Parse PT_DYNAMIC
        dynamic_tags = {}
        if dynamic_ph:
            f.seek(dynamic_ph['offset'])
            while True:
                d_tag, d_val = struct.unpack('<qQ', f.read(16))
                if d_tag == 0: break
                dynamic_tags.setdefault(d_tag, []).append(d_val)
                
        # Resolve String Table & Symbol Table from dynamic tags
        # DT_STRTAB = 5, DT_SYMTAB = 6, DT_STRSZ = 10, DT_SYMENT = 11
        # In Orbis OS: DT_SCE_STRTAB = 0x61000025, DT_SCE_SYMTAB = 0x61000027, etc.
        # Or DT_STRTAB may point to vaddr or file offset
        return phs, dynamic_tags

def search_strings_and_symbols(elf_path):
    print("\n--- String & Function Search ---")
    with open(elf_path, 'rb') as f:
        data = f.read()
        
    keywords = [
        b"scePadReadState", b"scePadOpen", b"scePadClose", b"scePadSetLightBar",
        b"sceVideoOutOpen", b"sceVideoOutSubmitFlip", b"sceVideoOutSetFlipRate",
        b"sceAudioOutOpen", b"sceAudioOutOutput",
        b"sceKernelAllocateDirectMemory", b"sceKernelMapDirectMemory",
        b"sceKernelCreateEqueue", b"sceKernelWaitEqueue",
        b"sceKernelCreateSema", b"sceKernelSignalSema", b"sceKernelWaitSema",
        b"scePthreadCreate", b"scePthreadJoin",
        b"sceFiosInitialize", b"sceFiosArchiveMount",
        b"libkernel", b"libScePad", b"libSceVideoOut", b"libSceAudioOut", b"libSceGnmDriver", b"libSceFios2",
        b"Gravity", b"Gravite", b"Hekseville", b"Jirga",
        b"delta_time", b"deltaTime", b"DeltaTime", b"fps", b"FPS"
    ]
    
    found = {}
    for kw in keywords:
        pos = 0
        hits = []
        while len(hits) < 5:
            idx = data.find(kw, pos)
            if idx == -1: break
            hits.append(hex(idx))
            pos = idx + len(kw)
        if hits:
            found[kw.decode('latin1')] = hits
            print(f"  [Found] '{kw.decode('latin1')}': {len(hits)} occurrence(s) at {hits}")
            
    return found

if __name__ == "__main__":
    elf = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\patch_extracted\CUSA04943-patch\eboot.elf"
    if not os.path.exists(elf):
        print(f"File not found: {elf}")
        sys.exit(1)
    phs, dyn = parse_elf(elf)
    search_strings_and_symbols(elf)
