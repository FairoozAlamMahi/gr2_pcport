import struct

BASE_PKG = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\PS4_Gravity_Rush 2_CUSA04943_01_Game_Full_psgames.by.pkg"

with open(BASE_PKG, "rb") as f:
    f.seek(0x010)
    file_count, entry_count = struct.unpack(">II", f.read(8))
    table_offset, = struct.unpack(">I", f.read(4))
    f.seek(table_offset)
    
    sfo_offset = None
    sfo_size = None
    for i in range(file_count):
        entry_data = f.read(32)
        eid, fn_offset, f1, f2, offset, size, pad = struct.unpack(">IIIIIIQ", entry_data)
        if eid == 0x1000:
            sfo_offset = offset
            sfo_size = size
            break

print(f"Base Game PARAM.SFO offset: {hex(sfo_offset)}, size: {sfo_size}")
