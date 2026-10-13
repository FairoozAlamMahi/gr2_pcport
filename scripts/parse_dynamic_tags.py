import os
import struct
import json

def parse_dynlib_data(elf_path):
    with open(elf_path, 'rb') as f:
        # Program headers
        f.seek(0x40) # e_phoff
        # e_phnum is at 0x38 (2 bytes)
        f.seek(0x38)
        phnum = struct.unpack('<H', f.read(2))[0]
        f.seek(0x40)
        
        dynlib_offset = None
        dynlib_size = None
        dynamic_offset = None
        dynamic_size = None
        
        for i in range(phnum):
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack('<IIQQQQQQ', f.read(56))
            if p_type == 2: # PT_DYNAMIC
                dynamic_offset = p_offset
                dynamic_size = p_filesz
            elif p_type == 0x61000000: # PT_SCE_DYNLIBDATA
                dynlib_offset = p_offset
                dynlib_size = p_filesz
                
        print(f"PT_DYNAMIC: offset 0x{dynamic_offset:x}, size 0x{dynamic_size:x}")
        print(f"PT_SCE_DYNLIBDATA: offset 0x{dynlib_offset:x}, size 0x{dynlib_size:x}")
        
        # Parse dynamic tags
        f.seek(dynamic_offset)
        dyn_entries = []
        while True:
            tag, val = struct.unpack('<qQ', f.read(16))
            if tag == 0: break
            dyn_entries.append((tag, val))
            
        tag_names = {
            1: "DT_NEEDED", 4: "DT_HASH", 5: "DT_STRTAB", 6: "DT_SYMTAB",
            7: "DT_RELA", 8: "DT_RELASZ", 9: "DT_RELAENT", 10: "DT_STRSZ",
            11: "DT_SYMENT", 21: "DT_DEBUG", 23: "DT_JMPREL", 24: "DT_BIND_NOW",
            0x61000013: "DT_SCE_EXPORT_LIB", 0x61000015: "DT_SCE_IMPORT_LIB",
            0x61000025: "DT_SCE_STRTAB", 0x61000027: "DT_SCE_SYMTAB",
            0x61000029: "DT_SCE_RELA", 0x6100002b: "DT_SCE_RELASZ",
            0x6100002d: "DT_SCE_JMPREL", 0x6100002f: "DT_SCE_PLTGOT"
        }
        
        print("\n--- Dynamic Tags ---")
        for tag, val in dyn_entries:
            t_name = tag_names.get(tag, f"TAG_0x{tag:x}")
            print(f"  {t_name:24s}: 0x{val:016x} ({val})")

parse_dynlib_data(r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\patch_extracted\CUSA04943-patch\eboot.elf")
