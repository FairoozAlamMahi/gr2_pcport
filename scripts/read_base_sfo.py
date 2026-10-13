import struct

BASE_PKG = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages\PS4_Gravity_Rush 2_CUSA04943_01_Game_Full_psgames.by.pkg"

def parse_sfo(sfo_bytes):
    magic, version, key_table_start, data_table_start, num_entries = struct.unpack("<4sIIII", sfo_bytes[:20])
    entries = {}
    entry_table = sfo_bytes[20:20 + num_entries * 16]
    for i in range(num_entries):
        k_off, fmt, length, max_len, d_off = struct.unpack("<HHIII", entry_table[i*16:(i+1)*16])
        key = sfo_bytes[key_table_start + k_off:].split(b"\x00")[0].decode("utf-8")
        val_bytes = sfo_bytes[data_table_start + d_off: data_table_start + d_off + length]
        if fmt == 0x0204 or fmt == 0x0004:
            val = val_bytes.rstrip(b"\x00").decode("utf-8", errors="ignore")
        elif fmt == 0x0404:
            val, = struct.unpack("<I", val_bytes[:4])
        else:
            val = val_bytes
        entries[key] = val
    return entries

with open(BASE_PKG, "rb") as f:
    f.seek(0x102ec0)
    sfo = parse_sfo(f.read(2228))
    print("PARAM.SFO Contents of Base Game PKG:")
    for k, v in sorted(sfo.items()):
        print(f"  {k}: {v}")
