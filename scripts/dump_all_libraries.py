import os
import struct

def dump_libs(elf_path):
    with open(elf_path, 'rb') as f:
        # Read program headers
        f.seek(0x38)
        phnum = struct.unpack('<H', f.read(2))[0]
        f.seek(0x40)
        
        dyn_off = None
        dynlib_off = None
        for _ in range(phnum):
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack('<IIQQQQQQ', f.read(56))
            if p_type == 2: dyn_off = p_offset
            elif p_type == 0x61000000: dynlib_off = p_offset

        # Parse tags
        f.seek(dyn_off)
        tags = []
        while True:
            t, v = struct.unpack('<qQ', f.read(16))
            if t == 0: break
            tags.append((t, v))
            
        # Find string table tag
        # DT_SCE_STRTAB = 0x61000025 or DT_STRTAB = 5
        strtab_off = None
        strsz = None
        for t, v in tags:
            if t in (5, 0x61000025):
                strtab_off = v
            elif t in (10, 0x61000027): # strsz
                strsz = v

        print(f"Strtab Tag: {strtab_off:#x}, Dynlib Off: {dynlib_off:#x}")
        
        # In Orbis, strtab offset is either absolute in file, or relative to PT_SCE_DYNLIBDATA
        # Let's check both
        candidates = [dynlib_off + (strtab_off if strtab_off else 0), strtab_off if strtab_off else 0, dynlib_off]
        
        # Or look at PT_SCE_DYNLIBDATA directly
        f.seek(dynlib_off)
        dynlib_raw = f.read(0x4000)
        
        # Read imported libraries from DT_SCE_IMPORT_LIB (0x61000015)
        import_libs = [v for t, v in tags if t == 0x61000015]
        print(f"Total DT_SCE_IMPORT_LIB entries: {len(import_libs)}")
        
        # Read string table within dynlib
        for imp in import_libs:
            str_idx = imp & 0xFFFFFFFF
            # print string from dynlib_raw at str_idx
            if str_idx < len(dynlib_raw):
                s = dynlib_raw[str_idx:].split(b'\x00')[0].decode('ascii', errors='ignore')
                print(f"  Import: {s} (id: {imp >> 32:#x}, str_off: {str_idx:#x})")

dump_libs(r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\patch_extracted\CUSA04943-patch\eboot.elf")
