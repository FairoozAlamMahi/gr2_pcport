#pragma once

#include "common/types.h"
#include <windows.h>
#include <string>
#include <unordered_map>
#include <mutex>

namespace Memory {

// Direct Memory (DMEM) Pool configuration
// Base PS4: 5120 MB (5 GB)
// PS4 Pro (Neo Mode): +4096 MB -> Total: 9216 MB (9 GB) or 9.5 GB
constexpr u64 DMEM_POOL_BASE_SIZE  = 5120ULL * 1024 * 1024;
constexpr u64 DMEM_EXTRA_PRO_SIZE  = 4096ULL * 1024 * 1024;
constexpr u64 TOTAL_DMEM_POOL_SIZE = DMEM_POOL_BASE_SIZE + DMEM_EXTRA_PRO_SIZE; // ~9.5 GB

class VirtualMemoryManager {
public:
    static VirtualMemoryManager& Instance();

    bool Initialize();
    void Shutdown();

    // Direct Memory (DMEM) allocation and aliasing
    u64 AllocateDirectMemory(u64 size, u64 alignment);
    bool ReleaseDirectMemory(u64 offset, u64 size);
    void* MapDirectMemory(u64 dmem_offset, u64 size, void* target_vaddr, u32 protection);
    bool UnmapDirectMemory(void* vaddr, u64 size);

    // Flexible Memory (system heap)
    void* AllocateFlexibleMemory(u64 size, u32 protection);
    bool FreeFlexibleMemory(void* vaddr, u64 size);

    // Virtual address translation & bounds checking
    inline bool IsGuestAddress(const void* ptr) const {
        return (reinterpret_cast<uintptr_t>(ptr) >= 0x10000ULL);
    }
    inline void* GuestToHost(u64 guest_vaddr) const {
        if (guest_vaddr < 0x10000ULL) return nullptr;
        return reinterpret_cast<void*>(guest_vaddr);
    }
    inline u64 HostToGuest(const void* host_ptr) const {
        return reinterpret_cast<u64>(host_ptr);
    }

    // Helpers
    u64 GetTotalDirectMemorySize() const { return TOTAL_DMEM_POOL_SIZE; }
    u64 GetAvailableDirectMemorySize() const;

private:
    VirtualMemoryManager() = default;
    ~VirtualMemoryManager() = default;

    std::mutex m_mutex;
    void* m_guest_arena_base{nullptr};
    HANDLE m_dmem_section{NULL};
    u64 m_dmem_allocated{0};

    struct DmemBlock {
        u64 offset;
        u64 size;
        bool in_use;
    };
    std::unordered_map<u64, DmemBlock> m_dmem_blocks;
};

} // namespace Memory
