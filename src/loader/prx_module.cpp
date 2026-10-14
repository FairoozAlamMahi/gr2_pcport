#include "loader/prx_module.h"
#include "loader/nid_table.h"
#include "loader/elf_loader.h"
#include "common/logger.h"
#include <fstream>
#include <windows.h>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace Loader {

#pragma pack(push, 1)
struct Elf64_Ehdr {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
};

struct Elf64_Phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
};

struct DynTag {
    s64 d_tag;
    u64 d_val;
};

struct Elf64_Rela {
    u64 r_offset;
    u64 r_info;
    s64 r_addend;
};

struct Elf64_Sym {
    u32 st_name;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
    u64 st_value;
    u64 st_size;
};
#pragma pack(pop)

bool PrxModule::Load(const std::string& prx_path, uintptr_t base_address) {
    m_name = fs::path(prx_path).filename().string();
    m_base_address = base_address;

    LOG_INFO("PRX", "Loading PRX Module: " + m_name + " at base address 0x" + std::to_string(m_base_address));

    std::ifstream file(prx_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("PRX", "Could not open PRX file: " + prx_path);
        return false;
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    m_file_buffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_file_buffer.data()), size)) {
        LOG_ERROR("PRX", "Failed to read PRX contents: " + prx_path);
        return false;
    }

    const u8* data = m_file_buffer.data();

    if (!ParseAndMapSegments(data, size)) {
        LOG_ERROR("PRX", "ParseAndMapSegments failed for " + m_name);
        return false;
    }

    RewriteTlsInstructions(data);
    RegisterExports(data);

    LOG_INFO("PRX", "Harvested " + std::to_string(m_exports_count) + " exported symbols from " + m_name);
    return true;
}

bool PrxModule::RelocateAndLink() {
    if (m_file_buffer.empty()) return false;
    return ApplyRelocations(m_file_buffer.data());
}

bool PrxModule::ParseAndMapSegments(const u8* data, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) return false;
    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') {
        LOG_ERROR("PRX", "Not a valid ELF binary: " + m_name);
        return false;
    }

    m_segments.clear();
    const auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(data + ehdr->e_phoff);

    for (u16 i = 0; i < ehdr->e_phnum; ++i) {
        const auto& p = phdrs[i];
        m_segments.push_back({
            p.p_type, p.p_flags, p.p_offset, p.p_vaddr, p.p_filesz, p.p_memsz
        });

        // Map PT_LOAD and PT_SCE_RELRO segments into guest arena
        if (p.p_type == 1 || p.p_type == 0x61000010) {
            uintptr_t dest_vaddr = m_base_address + p.p_vaddr;
            size_t mem_size = (p.p_memsz + 0xFFF) & ~0xFFFULL;

            void* alloc_ptr = VirtualAlloc(reinterpret_cast<void*>(dest_vaddr),
                                           mem_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!alloc_ptr) {
                LOG_ERROR("PRX", "Failed to map segment to 0x" + std::to_string(dest_vaddr));
                return false;
            }

            // Zero memory and copy segment file content
            std::memset(alloc_ptr, 0, mem_size);
            if (p.p_filesz > 0 && p.p_offset < size) {
                std::memcpy(alloc_ptr, data + p.p_offset, p.p_filesz);
            }
        }
    }

    return true;
}

bool PrxModule::RewriteTlsInstructions(const u8* data) {
    DWORD guest_tls_slot = ElfLoader::Instance().GetGuestTlsSlot();
    u32 disp32 = 0x1480 + 8 * guest_tls_slot;

    u32 rewritten = 0;
    for (const auto& seg : m_segments) {
        if (seg.type == 1 && (seg.flags & 1)) { // PT_LOAD with Execute permission
            u8* seg_ptr = reinterpret_cast<u8*>(m_base_address + seg.vaddr);
            size_t seg_size = seg.filesz;

            for (size_t i = 0; i + 9 <= seg_size; ++i) {
                if (seg_ptr[i] == 0x64 && seg_ptr[i+1] == 0x48 && seg_ptr[i+2] == 0x8B &&
                    seg_ptr[i+3] == 0x04 && seg_ptr[i+4] == 0x25 &&
                    seg_ptr[i+5] == 0x00 && seg_ptr[i+6] == 0x00 &&
                    seg_ptr[i+7] == 0x00 && seg_ptr[i+8] == 0x00) {

                    seg_ptr[i] = 0x65; // %gs prefix
                    *reinterpret_cast<u32*>(&seg_ptr[i+5]) = disp32;
                    rewritten++;
                    i += 8;
                }
            }
        }
    }

    if (rewritten > 0) {
        LOG_INFO("PRX", "Rewrote " + std::to_string(rewritten) + " TLS instructions in " + m_name);
    }
    return true;
}

bool PrxModule::ApplyRelocations(const u8* data) {
    u64 dynlib_offset = 0;
    u64 dynamic_offset = 0;

    for (const auto& seg : m_segments) {
        if (seg.type == 0x61000000) dynlib_offset = seg.offset;
        else if (seg.type == 2) dynamic_offset = seg.offset;
    }

    if (dynlib_offset == 0 || dynamic_offset == 0) return true;

    const u8* dynlib = data + dynlib_offset;

    u64 rela_off = 0, relasz = 0;
    u64 jmprel_off = 0, pltrelsz = 0;
    u64 symtab_off = 0, symtab_sz = 0;
    u64 strtab_off = 0, strtab_sz = 0;

    const auto* dyn_tags = reinterpret_cast<const DynTag*>(data + dynamic_offset);
    for (size_t i = 0;; ++i) {
        s64 tag = dyn_tags[i].d_tag;
        u64 val = dyn_tags[i].d_val;
        if (tag == 0) break;

        // Sony dynamic tags (SocraticBliss / shadPS4 spec)
        if (tag == 0x6100002F) rela_off = val;       // DT_SCE_RELA
        else if (tag == 0x61000031) relasz = val;   // DT_SCE_RELASZ
        else if (tag == 0x61000029) jmprel_off = val; // DT_SCE_JMPREL
        else if (tag == 0x6100002D) pltrelsz = val;  // DT_SCE_PLTRELSZ
        else if (tag == 0x61000039) symtab_off = val; // DT_SCE_SYMTAB
        else if (tag == 0x6100003F) symtab_sz = val;  // DT_SCE_SYMTABSZ
        else if (tag == 0x61000035) strtab_off = val; // DT_SCE_STRTAB
        else if (tag == 0x61000037) strtab_sz = val;  // DT_SCE_STRSZ
    }

    const u8* strtab = (strtab_off != 0) ? (dynlib + strtab_off) : (dynlib + 0x18);
    const auto* syms = (symtab_off != 0) ? reinterpret_cast<const Elf64_Sym*>(dynlib + symtab_off) : nullptr;
    u32 sym_count = (symtab_sz != 0) ? static_cast<u32>(symtab_sz / sizeof(Elf64_Sym)) : 0;

    auto IsOffsetMapped = [&](u64 off) {
        for (const auto& s : m_segments) {
            if ((s.type == 1 || s.type == 0x61000010) && off >= s.vaddr && off + 8 <= s.vaddr + s.memsz) {
                return true;
            }
        }
        return false;
    };

    u32 total_applied = 0;
    u32 plt_resolved = 0;
    auto ProcessTable = [&](u64 table_off, u64 table_sz) {
        if (table_off == 0 || table_sz == 0) return;
        const auto* entries = reinterpret_cast<const Elf64_Rela*>(dynlib + table_off);
        u32 count = static_cast<u32>(table_sz / sizeof(Elf64_Rela));

        for (u32 i = 0; i < count; ++i) {
            const auto& r = entries[i];
            u32 type = static_cast<u32>(r.r_info & 0xFFFFFFFF);
            u32 sym_idx = static_cast<u32>(r.r_info >> 32);

            if (!IsOffsetMapped(r.r_offset)) {
                continue;
            }
            u64* target = reinterpret_cast<u64*>(m_base_address + r.r_offset);

            uintptr_t sym_val = 0;
            if (sym_idx != 0 && syms && sym_idx < sym_count) {
                const auto& s = syms[sym_idx];
                if (s.st_shndx != 0) {
                    sym_val = m_base_address + s.st_value;
                } else if (s.st_name != 0 && (strtab_sz == 0 || s.st_name < strtab_sz)) {
                    const char* full_name = reinterpret_cast<const char*>(strtab + s.st_name);
                    std::string sym_str(full_name);
                    size_t hash_pos = sym_str.find('#');
                    std::string nid_prefix = (hash_pos != std::string::npos) ? sym_str.substr(0, hash_pos) : sym_str;

                    sym_val = NidTable::Instance().Resolve(nid_prefix, sym_str);
                    if (NidTable::Instance().HasSymbol(nid_prefix)) {
                        plt_resolved++;
                    }
                }
            }

            if (type == 8) { // R_X86_64_RELATIVE
                *target = m_base_address + r.r_addend;
                total_applied++;
            } else if (type == 1 || type == 6) { // R_X86_64_64, R_X86_64_GLOB_DAT
                if (sym_idx != 0) {
                    *target = sym_val + r.r_addend;
                } else {
                    *target = m_base_address + r.r_addend;
                }
                total_applied++;
            } else if (type == 7) { // R_X86_64_JUMP_SLOT
                *target = sym_val;
                total_applied++;
            } else if (type == 16) { // R_X86_64_TPOFF64
                u64 val = (syms && sym_idx < sym_count) ? syms[sym_idx].st_value : 0;
                *target = val + r.r_addend;
                total_applied++;
            }
        }
    };

    ProcessTable(rela_off, relasz);
    ProcessTable(jmprel_off, pltrelsz);

    LOG_INFO("PRX", "Relocated & Linked " + m_name + ": " + std::to_string(total_applied) + 
             " relocations (" + std::to_string(plt_resolved) + " PLT jump slots bound).");
    return true;
}

bool PrxModule::RegisterExports(const u8* data) {
    u64 dynlib_offset = 0;
    u64 dynamic_offset = 0;

    for (const auto& seg : m_segments) {
        if (seg.type == 0x61000000) dynlib_offset = seg.offset;
        else if (seg.type == 2) dynamic_offset = seg.offset;
    }

    if (dynlib_offset == 0 || dynamic_offset == 0) return false;

    const u8* dynlib = data + dynlib_offset;

    u64 symtab_off = 0, symtab_sz = 0;
    u64 strtab_off = 0, strtab_sz = 0;
    const auto* dyn_tags = reinterpret_cast<const DynTag*>(data + dynamic_offset);
    for (size_t i = 0;; ++i) {
        s64 tag = dyn_tags[i].d_tag;
        u64 val = dyn_tags[i].d_val;
        if (tag == 0) break;

        if (tag == 0x61000039) symtab_off = val;       // DT_SCE_SYMTAB
        else if (tag == 0x6100003F) symtab_sz = val;   // DT_SCE_SYMTABSZ
        else if (tag == 0x61000035) strtab_off = val;  // DT_SCE_STRTAB
        else if (tag == 0x61000037) strtab_sz = val;   // DT_SCE_STRSZ
    }

    if (symtab_off == 0 || symtab_sz == 0) return false;

    const u8* strtab = (strtab_off != 0) ? (dynlib + strtab_off) : (dynlib + 0x18);
    const auto* syms = reinterpret_cast<const Elf64_Sym*>(dynlib + symtab_off);
    u32 sym_count = static_cast<u32>(symtab_sz / sizeof(Elf64_Sym));

    for (u32 i = 0; i < sym_count; ++i) {
        const auto& s = syms[i];
        if (s.st_value != 0 && s.st_name != 0) {
            if (strtab_sz > 0 && s.st_name >= strtab_sz) continue;
            const char* full_name = reinterpret_cast<const char*>(strtab + s.st_name);
            std::string name_str(full_name);

            // NID format: 11 characters before '#'
            size_t hash_pos = name_str.find('#');
            std::string nid_prefix = (hash_pos != std::string::npos) ? name_str.substr(0, hash_pos) : name_str;

            uintptr_t func_vaddr = m_base_address + s.st_value;
            if (!NidTable::Instance().HasSymbol(nid_prefix)) {
                NidTable::Instance().RegisterSymbol(nid_prefix, func_vaddr, name_str);
                m_exports_count++;
            }
        }
    }

    return true;
}

} // namespace Loader
