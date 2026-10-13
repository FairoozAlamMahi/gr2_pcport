import os
import sys
import subprocess
import shutil
import struct

def log(msg):
    print(f"[PrepareGame] {msg}", flush=True)

def parse_sfo(path):
    with open(path, 'rb') as f:
        magic, version, key_off, data_off, count = struct.unpack('<4sIIII', f.read(20))
        entries = []
        for _ in range(count):
            k_off, fmt, length, max_len, d_off = struct.unpack('<HHIII', f.read(16))
            entries.append((k_off, fmt, length, max_len, d_off))
        
        results = {}
        for k_off, fmt, length, max_len, d_off in entries:
            f.seek(key_off + k_off)
            key = b''
            while True:
                ch = f.read(1)
                if ch == b'\x00' or not ch: break
                key += ch
            key = key.decode('ascii', errors='ignore')
            
            f.seek(data_off + d_off)
            raw = f.read(length)
            if fmt == 0x0204: # string
                val = raw.rstrip(b'\x00').decode('utf-8', errors='ignore')
            elif fmt == 0x0404: # int
                val = struct.unpack('<I', raw)[0]
            else:
                val = raw
            results[key] = val
        return results

def hardlink_tree(src_dir, dst_dir, overwrite=False):
    total_linked = 0
    total_overwritten = 0
    for root, dirs, files in os.walk(src_dir):
        rel_path = os.path.relpath(root, src_dir)
        target_root = os.path.join(dst_dir, rel_path)
        os.makedirs(target_root, exist_ok=True)
        
        for file in files:
            src_file = os.path.join(root, file)
            dst_file = os.path.join(target_root, file)
            
            if os.path.exists(dst_file):
                if overwrite:
                    os.remove(dst_file)
                    os.link(src_file, dst_file)
                    total_overwritten += 1
            else:
                os.link(src_file, dst_file)
                total_linked += 1
    return total_linked, total_overwritten

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    pkg_dir = os.path.abspath(os.path.join(repo_root, "..", "gr2_packages"))
    extractor_exe = os.path.join(repo_root, "tools", "shadps4_extractor", "ShadPs4Plus-PkgExtractor-1.1-windows", "pkg_extractor.exe")
    unfself_script = os.path.join(repo_root, "tools", "unfself.py")
    
    base_pkg = os.path.join(pkg_dir, "PS4_Gravity_Rush 2_CUSA04943_01_Game_Full_psgames.by.pkg")
    patch_pkg = os.path.join(pkg_dir, "PS4_Gravity_Rush 2_CUSA04943_02_Patch_v1.11_psgames.by.pkg")
    
    base_extract_dir = os.path.join(pkg_dir, "base_extracted")
    patch_extract_dir = os.path.join(pkg_dir, "patch_extracted")
    merged_dir = os.path.join(pkg_dir, "merged_game")
    
    if not os.path.exists(base_pkg) or not os.path.exists(patch_pkg):
        log(f"ERROR: Base PKG or Patch PKG not found in {pkg_dir}")
        sys.exit(1)
        
    log("Step 1: Extracting Base Game PKG (if needed)...")
    base_target = os.path.join(base_extract_dir, "CUSA04943-app")
    if not os.path.exists(base_target) or not os.path.exists(os.path.join(base_target, "eboot.bin")):
        os.makedirs(base_extract_dir, exist_ok=True)
        log(f"Extracting {base_pkg} -> {base_extract_dir} ...")
        res = subprocess.run([extractor_exe, base_pkg, base_extract_dir], capture_output=True, text=True)
        if res.returncode != 0:
            log(f"Extraction failed: {res.stderr}")
            sys.exit(1)
        log("Base game extraction complete!")
    else:
        log("Base game already extracted. Skipping.")

    log("Step 2: Extracting Patch v1.11 PKG (if needed)...")
    patch_target = os.path.join(patch_extract_dir, "CUSA04943-patch")
    if not os.path.exists(patch_target) or not os.path.exists(os.path.join(patch_target, "eboot.bin")):
        os.makedirs(patch_extract_dir, exist_ok=True)
        log(f"Extracting {patch_pkg} -> {patch_extract_dir} ...")
        res = subprocess.run([extractor_exe, patch_pkg, patch_extract_dir], capture_output=True, text=True)
        if res.returncode != 0:
            log(f"Extraction failed: {res.stderr}")
            sys.exit(1)
        log("Patch v1.11 extraction complete!")
    else:
        log("Patch v1.11 already extracted. Skipping.")

    log("Step 3: Creating Merged Game Directory via NTFS Hardlinks...")
    os.makedirs(merged_dir, exist_ok=True)
    
    # 3a. Link base files
    linked_base, _ = hardlink_tree(base_target, merged_dir, overwrite=False)
    log(f"Hardlinked {linked_base} files from Base Game.")
    
    # 3b. Overlay patch files
    linked_patch, overwritten = hardlink_tree(patch_target, merged_dir, overwrite=True)
    log(f"Overlaid Patch v1.11: {linked_patch} new files linked, {overwritten} base files updated.")

    log("Step 4: Decrypting Merged eboot.bin -> eboot.elf...")
    merged_eboot_bin = os.path.join(merged_dir, "eboot.bin")
    merged_eboot_elf = os.path.join(merged_dir, "eboot.elf")
    if not os.path.exists(merged_eboot_elf):
        res = subprocess.run([sys.executable, unfself_script, merged_eboot_bin, merged_eboot_elf], capture_output=True, text=True)
        if res.returncode != 0 or not os.path.exists(merged_eboot_elf):
            log(f"unfself failed: {res.stderr}")
            sys.exit(1)
        log(f"Successfully generated {merged_eboot_elf} ({os.path.getsize(merged_eboot_elf)} bytes)")
    else:
        log("eboot.elf already present in merged game.")

    log("Step 5: Verifying Merged param.sfo...")
    sfo_path = os.path.join(merged_dir, "sce_sys", "param.sfo")
    if os.path.exists(sfo_path):
        sfo_info = parse_sfo(sfo_path)
        log(f"Title ID: {sfo_info.get('TITLE_ID')}")
        log(f"App Version: {sfo_info.get('APP_VER')}")
        log(f"Title: {sfo_info.get('TITLE')}")
        assert sfo_info.get('APP_VER') == '01.11', f"Expected APP_VER 01.11, got {sfo_info.get('APP_VER')}"
        log("SUCCESS: Merged game verified at Update v1.11!")
    else:
        log("WARNING: param.sfo not found in merged game sce_sys!")

    # Step 6: Create local link in gr2port for runner access
    local_game_dir = os.path.join(repo_root, "game")
    os.makedirs(local_game_dir, exist_ok=True)
    local_elf = os.path.join(local_game_dir, "eboot.elf")
    if not os.path.exists(local_elf):
        try:
            os.link(merged_eboot_elf, local_elf)
            log(f"Hardlinked eboot.elf into local repository {local_elf}")
        except Exception as e:
            shutil.copy2(merged_eboot_elf, local_elf)
            log(f"Copied eboot.elf into local repository {local_elf}")

    log("==================================================")
    log(" GAME PREPARATION & HARDLINK MERGE COMPLETE!")
    log("==================================================")

if __name__ == "__main__":
    main()
