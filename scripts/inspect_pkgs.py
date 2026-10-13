import os
import struct
import sys

PKG_DIR = r"C:\Users\faalv\Desktop\Personal project\gravityrush2port\gr2_packages"

def parse_pkg_header(file_path):
    with open(file_path, "rb") as f:
        magic = f.read(4)
        if magic != b"\x7FCNT":
            return {"valid": False, "error": f"Invalid magic: {magic}"}
        
        f.seek(0x008)
        header_size, = struct.unpack(">I", f.read(4))
        f.seek(0x010)
        file_count, entry_count = struct.unpack(">II", f.read(8))
        table_offset, = struct.unpack(">I", f.read(4))
        f.seek(0x020)
        body_offset, body_size = struct.unpack(">QQ", f.read(16))
        f.seek(0x040)
        content_id = f.read(36).decode("utf-8", errors="ignore").rstrip("\x00")
        
        return {
            "valid": True,
            "filename": os.path.basename(file_path),
            "size_gb": round(os.path.getsize(file_path) / (1024**3), 2),
            "content_id": content_id,
            "file_count": file_count,
            "entry_count": entry_count,
            "header_size": header_size,
            "table_offset": hex(table_offset),
            "body_offset": hex(body_offset),
            "body_size_gb": round(body_size / (1024**3), 2)
        }

if __name__ == "__main__":
    print(f"Scanning PKG files in: {PKG_DIR}")
    for fname in sorted(os.listdir(PKG_DIR)):
        if fname.lower().endswith(".pkg"):
            fpath = os.path.join(PKG_DIR, fname)
            info = parse_pkg_header(fpath)
            print(f"\n--- {info['filename']} ---")
            for k, v in info.items():
                if k != "filename":
                    print(f"  {k}: {v}")
