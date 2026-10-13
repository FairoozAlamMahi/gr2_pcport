#include "loader/elf_loader.h"
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
        }
    }

    // Allocate Windows TLS slot for guest TCB
    m_tls_info.win_tls_slot = TlsAlloc();
    LOG_INFO("LOADER", "Allocated Windows TLS slot for Guest TCB: " + 
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
    // Allocate Thread Control Block (TCB) + TLS block for guest thread
    usize alloc_size = m_tls_info.memsz + 0x1000;
    u8* tcb_block = reinterpret_cast<u8*>(VirtualAlloc(
        nullptr, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!tcb_block) return nullptr;

    // Copy master TLS template if available
    if (m_tls_info.vaddr != 0 && m_tls_info.filesz > 0) {
        void* src_tls = reinterpret_cast<void*>(m_base_address + m_tls_info.vaddr);
        std::memcpy(tcb_block, src_tls, m_tls_info.filesz);
    }

    // Set self-pointer at tcb_block[0]
    *reinterpret_cast<void**>(tcb_block) = tcb_block;

    // Assign to host Windows TLS slot for this thread
    TlsSetValue(m_tls_info.win_tls_slot, tcb_block);

    return tcb_block;
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
    #pragma pack(pop)

    const auto* dyn_tags = reinterpret_cast<const DynTag*>(data + dynamic_offset);
    for (size_t i = 0;; ++i) {
        s64 tag = dyn_tags[i].d_tag;
        u64 val = dyn_tags[i].d_val;
        if (tag == 0) break;

        if (tag == 0x61000031) rela_off = val;
        else if (tag == 0x61000039) relasz = val;
        else if (tag == 0x6100002f) jmprel_off = val;
        else if (tag == 0x6100003f) pltrelsz = val;
    }

    u32 total_applied = 0;

    auto ProcessRelas = [&](u64 table_off, u64 table_sz) {
        if (table_off == 0 || table_sz == 0) return;
        const auto* entries = reinterpret_cast<const Elf64_Rela*>(dynlib + table_off);
        u32 count = static_cast<u32>(table_sz / sizeof(Elf64_Rela));

        for (u32 i = 0; i < count; ++i) {
            const auto& r = entries[i];
            u32 type = static_cast<u32>(r.r_info & 0xFFFFFFFF);
            u64* target = reinterpret_cast<u64*>(m_base_address + r.r_offset);

            if (type == 8) { // R_X86_64_RELATIVE
                *target = m_base_address + r.r_addend;
                total_applied++;
            } else if (type == 1 || type == 6) { // R_X86_64_64 or R_X86_64_GLOB_DAT
                *target = m_base_address + r.r_addend;
                total_applied++;
            }
        }
    };

    ProcessRelas(rela_off, relasz);
    ProcessRelas(jmprel_off, pltrelsz);

    LOG_INFO("LOADER", "Successfully applied " + std::to_string(total_applied) + 
             " AMD64 relocations (rebased to 0x" + std::to_string(m_base_address) + ").");
    return true;
}

} // namespace Loader
