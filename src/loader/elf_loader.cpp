#include "loader/elf_loader.h"
#include "loader/nid_table.h"
#include "common/logger.h"
#include "memory/vm.h"
#include <fstream>
#include <cstring>

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
#pragma pack(pop)

ElfLoader& ElfLoader::Instance() {
    static ElfLoader s_instance;
    return s_instance;
}

bool ElfLoader::Load(const std::string& elf_path) {
    LOG_INFO("LOADER", "Opening Orbis ELF executable: " + elf_path);

    std::ifstream file(elf_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_FATAL("LOADER", "Could not open ELF file: " + elf_path);
        return false;
    }

    usize file_size = static_cast<usize>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(file_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        LOG_FATAL("LOADER", "Failed to read ELF file into memory!");
        return false;
    }

    if (!ParseHeaders(buffer.data(), file_size)) {
        return false;
    }

    if (!MapSegments(buffer.data())) {
        return false;
    }

    if (!PatchTlsInstructions()) {
        return false;
    }

    if (!ApplyRelocations(buffer.data())) {
        return false;
    }

    if (!ResolveImports(buffer.data())) {
        return false;
    }

    LOG_INFO("LOADER", "ELF loaded successfully. Entry point: " + 
             std::to_string(reinterpret_cast<uintptr_t>(m_entry_point)));
    return true;
}

bool ElfLoader::ParseHeaders(const u8* data, usize size) {
    if (size < sizeof(Elf64_Ehdr)) {
        LOG_ERROR("LOADER", "File too small to contain ELF header");
        return false;
    }

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(data);

    if (std::memcmp(ehdr->e_ident, "\x7F" "ELF", 4) != 0) {
        LOG_ERROR("LOADER", "Invalid ELF magic");
        return false;
    }

    if (ehdr->e_ident[4] != 2) { // ELFCLASS64
        LOG_ERROR("LOADER", "ELF is not 64-bit");
        return false;
    }

    if (ehdr->e_machine != 0x3E) { // EM_X86_64
        LOG_ERROR("LOADER", "ELF is not AMD64 / x86-64 architecture");
        return false;
    }

    LOG_INFO("LOADER", "Header verified: FreeBSD/Orbis AMD64 ELF (Type: 0x" + 
             std::to_string(ehdr->e_type) + ")");

    m_segments.clear();
    const auto* ph_base = reinterpret_cast<const Elf64_Phdr*>(data + ehdr->e_phoff);

    for (u16 i = 0; i < ehdr->e_phnum; ++i) {
        const auto& ph = ph_base[i];
        ElfSegment seg{};
        seg.type   = ph.p_type;
        seg.flags  = ph.p_flags;
        seg.offset = ph.p_offset;
        seg.vaddr  = ph.p_vaddr;
        seg.filesz = ph.p_filesz;
        seg.memsz  = ph.p_memsz;
        seg.align  = ph.p_align;
        m_segments.push_back(seg);

        if (ph.p_type == 7) { // PT_TLS
            m_tls_info.vaddr  = ph.p_vaddr;
            m_tls_info.filesz = ph.p_filesz;
            m_tls_info.memsz  = ph.p_memsz;
            m_tls_info.align  = ph.p_align;
            LOG_INFO("LOADER", "Found PT_TLS segment: filesz=0x" + 
                     std::to_string(ph.p_filesz) + ", memsz=0x" + std::to_string(ph.p_memsz));
        } else if (ph.p_type == 0x61000001) { // PT_SCE_PROCPARAM
            m_proc_param_vaddr = ph.p_vaddr;
            LOG_INFO("LOADER", "Found PT_SCE_PROCPARAM segment at VA 0x" + 
                     std::to_string(ph.p_vaddr));
        }
    }

    // Use pre-allocated Windows TLS slot if available
    if (m_tls_info.win_tls_slot == TLS_OUT_OF_INDEXES) {
        m_tls_info.win_tls_slot = TlsAlloc();
    }
    LOG_INFO("LOADER", "Windows TLS slot for Guest TCB: " + 
             std::to_string(m_tls_info.win_tls_slot));

    m_entry_point = reinterpret_cast<void*>(m_base_address + ehdr->e_entry);
    return true;
}

bool ElfLoader::MapSegments(const u8* data) {
    for (auto& seg : m_segments) {
        if (seg.type != 1 && seg.type != 0x61000010) { // PT_LOAD and PT_SCE_RELRO
            continue;
        }

        void* dest_addr = reinterpret_cast<void*>(m_base_address + seg.vaddr);

        DWORD prot = PAGE_READWRITE;
        if ((seg.flags & 1) && (seg.flags & 4)) {
            prot = PAGE_EXECUTE_READWRITE; // RX committed as RWX for runtime patching
        } else if (seg.flags & 4) {
            prot = PAGE_READWRITE;
        }

        void* mapped = VirtualAlloc(dest_addr, seg.memsz, MEM_COMMIT, prot);
        if (!mapped) {
            LOG_FATAL("LOADER", "Failed to commit virtual memory for segment at 0x" + 
                      std::to_string(reinterpret_cast<uintptr_t>(dest_addr)));
            return false;
        }

        // Copy file content
        if (seg.filesz > 0) {
            std::memcpy(dest_addr, data + seg.offset, seg.filesz);
        }

        // Zero out BSS section
        if (seg.memsz > seg.filesz) {
            std::memset(reinterpret_cast<u8*>(dest_addr) + seg.filesz, 0, seg.memsz - seg.filesz);
        }

        seg.host_addr = dest_addr;
        LOG_INFO("LOADER", "Mapped segment to 0x" + 
                 std::to_string(reinterpret_cast<uintptr_t>(dest_addr)) + 
                 " (" + std::to_string(seg.memsz) + " bytes)");
    }

    return true;
}

bool ElfLoader::PatchTlsInstructions() {
    if (m_tls_info.win_tls_slot == TLS_OUT_OF_INDEXES) {
        LOG_ERROR("LOADER", "Cannot patch TLS instructions: invalid Windows TLS slot");
        return false;
    }

    // Displacement inside 64-bit Windows TEB for TlsSlots[slot] is 0x1480 + 8 * slot
    u32 teb_displacement = 0x1480 + 8 * m_tls_info.win_tls_slot;
    u32 patch_count = 0;

    LOG_INFO("LOADER", "Scanning executable segments to rewrite %fs:[0] -> %gs:[0x" + 
             std::to_string(teb_displacement) + "] ...");

    for (const auto& seg : m_segments) {
        if (seg.type != 1 || !(seg.flags & 1)) { // Only executable PT_LOAD
            continue;
        }

        u8* code = reinterpret_cast<u8*>(seg.host_addr);
        usize size = seg.filesz;

        // Scan for 9-byte instruction pattern:
        // 64 48/49/4C/4D 8B [reg] 25 00 00 00 00 (mov reg, fs:[0])
        for (usize i = 0; i + 9 <= size; ++i) {
            if (code[i] == 0x64 &&                             // fs: prefix
                (code[i + 1] >= 0x48 && code[i + 1] <= 0x4F) && // REX prefix
                code[i + 2] == 0x8B &&                         // mov reg, [addr]
                code[i + 4] == 0x25 &&                         // SIB byte for disp32
                code[i + 5] == 0x00 && code[i + 6] == 0x00 &&
                code[i + 7] == 0x00 && code[i + 8] == 0x00) {

                // Rewrite prefix from fs (0x64) to gs (0x65)
                code[i] = 0x65;

                // Rewrite disp32 from 0x00000000 to teb_displacement
                std::memcpy(&code[i + 5], &teb_displacement, sizeof(u32));

                patch_count++;
            }
        }
    }

    LOG_INFO("LOADER", "Successfully rewritten " + std::to_string(patch_count) + 
             " Orbis TLS instructions to Windows TEB slots.");
    return true;
}

void* ElfLoader::SetupThreadTCB() {
    // In AMD64 ELF (System V / Orbis), thread-local variables are addressed
    // via negative offsets from the Thread Control Block (fs:[0]).
    // Allocate 64 KB block and place the TCB pointer at offset +0x8000 so that
    // both negative offsets (-32 KB) and positive offsets (+32 KB) are valid.
    constexpr usize TOTAL_ALLOC = 64 * 1024;
    constexpr usize TCB_OFFSET  = 32 * 1024;

    u8* raw_block = reinterpret_cast<u8*>(VirtualAlloc(
        nullptr, TOTAL_ALLOC, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!raw_block) return nullptr;

    u8* tcb_ptr = raw_block + TCB_OFFSET;

    // In ELF Variant II, static TLS template data sits before the TCB: [tcb_ptr - memsz .. tcb_ptr)
    if (m_tls_info.vaddr != 0 && m_tls_info.filesz > 0) {
        void* src_tls = reinterpret_cast<void*>(m_base_address + m_tls_info.vaddr);
        usize copy_sz = (std::min)(static_cast<usize>(m_tls_info.filesz), static_cast<usize>(TCB_OFFSET));
        std::memcpy(tcb_ptr - m_tls_info.memsz, src_tls, copy_sz);
    }

    // Set self-pointer at tcb_ptr[0] (fs:[0])
    *reinterpret_cast<void**>(tcb_ptr) = tcb_ptr;

    // Assign to host Windows TLS slot for this thread
    TlsSetValue(m_tls_info.win_tls_slot, tcb_ptr);

    return tcb_ptr;
}

bool ElfLoader::ApplyRelocations(const u8* data) {
    u64 dynlib_offset = 0;
    u64 dynamic_offset = 0;

    for (const auto& seg : m_segments) {
        if (seg.type == 0x61000000) { // PT_SCE_DYNLIBDATA
            dynlib_offset = seg.offset;
        } else if (seg.type == 2) { // PT_DYNAMIC
            dynamic_offset = seg.offset;
        }
    }

    if (dynlib_offset == 0 || dynamic_offset == 0) {
        LOG_WARN("LOADER", "No PT_SCE_DYNLIBDATA or PT_DYNAMIC found for relocations.");
        return true;
    }

    const u8* dynlib = data + dynlib_offset;

    u64 rela_off = 0;
    u64 relasz = 0;
    u64 jmprel_off = 0;
    u64 pltrelsz = 0;
    u64 symtab_off = 0;
    u64 symtab_sz = 0;
    u64 strtab_off = 0;
    u64 strtab_sz = 0;

    #pragma pack(push, 1)
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

    const auto* dyn_tags = reinterpret_cast<const DynTag*>(data + dynamic_offset);
    for (size_t i = 0;; ++i) {
        s64 tag = dyn_tags[i].d_tag;
        u64 val = dyn_tags[i].d_val;
        if (tag == 0) break;

        if (tag == 0x6100002F) rela_off = val;       // DT_SCE_RELA
        else if (tag == 0x61000031) relasz = val;   // DT_SCE_RELASZ
        else if (tag == 0x61000029) jmprel_off = val; // DT_SCE_JMPREL
        else if (tag == 0x6100002D) pltrelsz = val;  // DT_SCE_PLTRELSZ
        else if (tag == 0x61000039) symtab_off = val; // DT_SCE_SYMTAB
        else if (tag == 0x6100003F) symtab_sz = val;  // DT_SCE_SYMTABSZ
        else if (tag == 0x61000035) strtab_off = val; // DT_SCE_STRTAB
        else if (tag == 0x61000037) strtab_sz = val;  // DT_SCE_STRSZ
    }

    const u8* strtab = dynlib + ((strtab_off != 0) ? strtab_off : 0x18);
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
    u32 stubbed_count = 0;

    auto ProcessRelas = [&](u64 table_off, u64 table_sz) {
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
                    const char* full_sym = reinterpret_cast<const char*>(strtab + s.st_name);
                    std::string sym_str(full_sym);
                    size_t hash_pos = sym_str.find('#');
                    std::string nid_prefix = (hash_pos != std::string::npos) ? sym_str.substr(0, hash_pos) : sym_str;

                    sym_val = NidTable::Instance().Resolve(nid_prefix, sym_str);
                    if (NidTable::Instance().HasSymbol(nid_prefix)) {
                        plt_resolved++;
                    } else {
                        stubbed_count++;
                    }
                }
            }

            if (type == 8) { // R_X86_64_RELATIVE
                *target = m_base_address + r.r_addend;
                total_applied++;
            } else if (type == 1 || type == 6) { // R_X86_64_64 or R_X86_64_GLOB_DAT
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

    ProcessRelas(rela_off, relasz);
    ProcessRelas(jmprel_off, pltrelsz);

    LOG_INFO("LOADER", "Applied " + std::to_string(total_applied) + 
             " AMD64 relocations (" + std::to_string(plt_resolved) + " direct NID imports bound, " +
             std::to_string(stubbed_count) + " fallback stubs).");
    return true;
}

bool ElfLoader::ResolveImports(const u8* data) {
    // Relocations and imports are now fully resolved in ApplyRelocations via standard AMD64 ELF JMPREL/RELA
    return true;
}

bool ElfLoader::ExecuteProcParamInitializers() {
    if (m_proc_param_vaddr == 0) {
        LOG_INFO("LOADER", "No PT_SCE_PROCPARAM segment found.");
        return true;
    }

    auto ToHostPtr = [&](u64 addr) -> const u8* {
        if (addr >= m_base_address) {
            return reinterpret_cast<const u8*>(addr);
        }
        return reinterpret_cast<const u8*>(m_base_address + addr);
    };

    const u8* proc_param_bytes = ToHostPtr(m_proc_param_vaddr);
    // At m_proc_param_vaddr: offset +0x38 is pointer to sce_process_param (0x1660000)
    u64 sce_proc_param_va = *reinterpret_cast<const u64*>(proc_param_bytes + 0x38);
    if (sce_proc_param_va == 0) {
        LOG_WARN("LOADER", "SceProcParam address is NULL in SceKernelProcParam.");
        return true;
    }

    // In sce_process_param at 0x1660000: offset +0x30 is init_array_va (0x1660098)
    const u8* sce_proc_param_ptr = ToHostPtr(sce_proc_param_va);
    u64 init_table_va = *reinterpret_cast<const u64*>(sce_proc_param_ptr + 0x30);
    LOG_INFO("LOADER", "Executing SceProcParam priority initializers at VA 0x" + 
             std::to_string(init_table_va) + "...");

    using InitFunc = void (PS4_SYSV_ABI *)();

    while (init_table_va != 0) {
        const u8* table_ptr = ToHostPtr(init_table_va);
        u64 total_size = *reinterpret_cast<const u64*>(table_ptr);
        u64 priority = *reinterpret_cast<const u64*>(table_ptr + 8);
        if (total_size == 0 || total_size < 16) break;
        if (priority == 0 || priority > 10) break; // Reached end of valid priority tables

        if (priority != 1) {
            // Only Priority 1 runtime initializers are executed prior to entry point
            init_table_va += total_size;
            continue;
        }

        // For Priority 1 Heap API table:
        // fn[0] is the master allocator initializer (malloc_init / 0x466ff0).
        // Entries 1..count-1 are the heap API methods (finalize, malloc, free, etc.)
        // which must NOT be called as initializers (calling fn[1] destroys mspace!).
        u64 fn_va = *reinterpret_cast<const u64*>(table_ptr + 16);
        if (fn_va != 0) {
            LOG_INFO("LOADER", "Calling SceProcParam Priority " + std::to_string(priority) + 
                     " Init [0] at 0x" + std::to_string(fn_va) + "...");
            auto fn = reinterpret_cast<InitFunc>(const_cast<u8*>(ToHostPtr(fn_va)));
            fn();
        }

        init_table_va += total_size;
    }

    LOG_INFO("LOADER", "All SceProcParam initializers executed successfully.");
    return true;
}

} // namespace Loader
