#pragma once

#include "common/types.h"
#include <string>
#include <vector>
#include <windows.h>

namespace Loader {

struct ElfSegment {
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 filesz;
    u64 memsz;
    u64 align;
    void* host_addr{nullptr};
};

struct TlsInfo {
    u64 vaddr{0};
    u64 filesz{0};
    u64 memsz{0};
    u64 align{0};
    DWORD win_tls_slot{TLS_OUT_OF_INDEXES};
};

class ElfLoader {
public:
    static ElfLoader& Instance();

    bool Load(const std::string& elf_path);
    void* GetEntryPoint() const { return m_entry_point; }
    u64 GetBaseAddress() const { return m_base_address; }
    const TlsInfo& GetTlsInfo() const { return m_tls_info; }
    DWORD GetGuestTlsSlot() const { return m_tls_info.win_tls_slot; }
    void SetGuestTlsSlot(DWORD slot) { m_tls_info.win_tls_slot = slot; }

    // Prepare Thread Control Block (TCB) for a new guest thread
    void* SetupThreadTCB();

    // SceProcParam execution
    u64 GetProcParamAddress() const { return m_base_address + m_proc_param_vaddr; }
    bool ExecuteProcParamInitializers();

private:
    ElfLoader() = default;
    ~ElfLoader() = default;

    bool ParseHeaders(const u8* data, usize size);
    bool MapSegments(const u8* data);
    bool PatchTlsInstructions();
    bool ApplyRelocations(const u8* data);
    bool ResolveImports(const u8* data);

    u64 m_base_address{GUEST_ARENA_BASE};
    void* m_entry_point{nullptr};
    std::vector<ElfSegment> m_segments;
    TlsInfo m_tls_info;
    u64 m_proc_param_vaddr{0};
};

} // namespace Loader
