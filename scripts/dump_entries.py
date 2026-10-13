import os
import struct

PATCH_PKG = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\PS4_Gravity_Rush 2_CUSA04943_02_Patch_v1.11_psgames.by.pkg"

ENTRY_NAMES = {
    0x0001: "digests",
    0x0010: "entry_keys",
    0x0020: "image_key",
    0x0080: "general_digests",
    0x0100: "metas",
    0x0200: "entry_names",
    0x1000: "param.sfo",
    0x1001: "playgo-chunk.dat",
    0x1002: "playgo-chunk.sha",
    0x1003: "playgo-manifest.xml",
    0x1004: "pronunciation.xml",
    0x1005: "pronunciation.sig",
    0x1006: "pic1.png",
    0x1007: "pubtoolinfo.dat",
    0x1008: "app/playgo-chunk.dat",
    0x1200: "icon0.png",
    0x1220: "pic0.png",
    0x1240: "snd0.at9",
    0x1260: "changeinfo/changeinfo.xml",
    0x10000: "license.dat",
    0x10001: "license.info",
}

with open(PATCH_PKG, "rb") as f:
    f.seek(0x010)
    file_count, entry_count = struct.unpack(">II", f.read(8))
    table_offset, = struct.unpack(">I", f.read(4))
    
    print(f"File count: {file_count}, Entry count: {entry_count}, Table offset: {hex(table_offset)}")
    f.seek(table_offset)
    
    entries = []
    for i in range(file_count):
        entry_data = f.read(32)
        if len(entry_data) < 32:
            break
        eid, fn_offset, f1, f2, offset, size, pad = struct.unpack(">IIIIIIQ", entry_data)
        name = ENTRY_NAMES.get(eid, f"entry_{hex(eid)}")
        entries.append({
            "id": hex(eid),
            "name": name,
            "offset": hex(offset),
            "size": size,
            "size_kb": round(size / 1024, 2)
        })

print("\nFound entries:")
for e in entries:
    print(f"  ID: {e['id']:<10} Offset: {e['offset']:<12} Size: {e['size_kb']:<10} KB  Name: {e['name']}")
