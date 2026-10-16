#include "memory/vm.h"
#include "common/logger.h"
#include <system_error>

namespace Memory {

VirtualMemoryManager& VirtualMemoryManager::Instance() {
    static VirtualMemoryManager s_instance;
    return s_instance;
}

bool VirtualMemoryManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_INFO("MEMORY", "Initializing 1 TiB Guest Virtual Address Space...");

    // Reserve 1 TiB starting at 0x10000000000ULL
    m_guest_arena_base = VirtualAlloc(
        reinterpret_cast<void*>(GUEST_ARENA_BASE),
        GUEST_ARENA_SIZE,
        MEM_RESERVE,
        PAGE_NOACCESS
    );

    if (!m_guest_arena_base) {
        // Fallback: let Windows choose if fixed address fails
        LOG_WARN("MEMORY", "Fixed arena allocation at 0x10000000000ULL failed, trying dynamic allocation...");
        m_guest_arena_base = VirtualAlloc(
            nullptr,
            GUEST_ARENA_SIZE,
            MEM_RESERVE,
            PAGE_NOACCESS
        );
        if (!m_guest_arena_base) {
            LOG_FATAL("MEMORY", "Failed to reserve 1 TiB virtual arena!");
            return false;
        }
    }

    LOG_INFO("MEMORY", "Guest Virtual Arena successfully reserved at 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(m_guest_arena_base)));

    // Create backing pagefile section for Direct Memory pool (9.5 GB for PS4 Pro / Neo mode)
    ULARGE_INTEGER sec_size;
    sec_size.QuadPart = TOTAL_DMEM_POOL_SIZE;

    m_dmem_section = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_EXECUTE_READWRITE | SEC_COMMIT,
        sec_size.HighPart,
        sec_size.LowPart,
        nullptr
    );

    if (!m_dmem_section) {
        LOG_WARN("MEMORY", "CreateFileMapping for 9.5 GB DMEM with SEC_COMMIT failed, retrying with SEC_RESERVE...");
        m_dmem_section = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_EXECUTE_READWRITE | SEC_RESERVE,
            sec_size.HighPart,
            sec_size.LowPart,
            nullptr
        );
    }

    if (!m_dmem_section) {
        LOG_ERROR("MEMORY", "Failed to create backing section for Direct Memory pool!");
        return false;
    }

    LOG_INFO("MEMORY", "Direct Memory (DMEM) pool configured: " + 
             std::to_string(TOTAL_DMEM_POOL_SIZE / (1024 * 1024)) + " MB (PS4 Pro / Neo mode enabled)");

    return true;
}

void VirtualMemoryManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_dmem_section) {
        CloseHandle(m_dmem_section);
        m_dmem_section = NULL;
    }
    if (m_guest_arena_base) {
        VirtualFree(m_guest_arena_base, 0, MEM_RELEASE);
        m_guest_arena_base = nullptr;
    }
    LOG_INFO("MEMORY", "Virtual memory manager shut down.");
}

u64 VirtualMemoryManager::AllocateDirectMemory(u64 size, u64 alignment) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Align size to page boundary (64 KB or 2 MB superpage)
    u64 align = alignment > 0 ? alignment : 64 * 1024;
    u64 aligned_size = (size + align - 1) & ~(align - 1);

    if (m_dmem_allocated + aligned_size > TOTAL_DMEM_POOL_SIZE) {
        LOG_ERROR("MEMORY", "Out of Direct Memory! Requested: " + std::to_string(aligned_size) + 
                            ", Available: " + std::to_string(TOTAL_DMEM_POOL_SIZE - m_dmem_allocated));
        return ~0ULL;
    }

    u64 offset = m_dmem_allocated;
    m_dmem_allocated += aligned_size;

    m_dmem_blocks[offset] = { offset, aligned_size, true };
    return offset;
}

bool VirtualMemoryManager::ReleaseDirectMemory(u64 offset, u64 size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_dmem_blocks.find(offset);
    if (it != m_dmem_blocks.end()) {
        it->second.in_use = false;
        return true;
    }
    return false;
}

void* VirtualMemoryManager::MapDirectMemory(u64 dmem_offset, u64 size, void* target_vaddr, u32 protection) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_dmem_section) return nullptr;

    DWORD win_prot = PAGE_READWRITE;
    DWORD map_access = FILE_MAP_READ | FILE_MAP_WRITE;

    if (protection & 1) { // Execute
        win_prot = PAGE_EXECUTE_READWRITE;
        map_access |= FILE_MAP_EXECUTE;
    }

    // If target address is specified, map view there
    void* mapped = MapViewOfFileEx(
        m_dmem_section,
        map_access,
        static_cast<DWORD>(dmem_offset >> 32),
        static_cast<DWORD>(dmem_offset & 0xFFFFFFFF),
        size,
        target_vaddr
    );

    if (mapped) {
        // Ensure mapped view pages are committed if backing section was reserved
        VirtualAlloc(mapped, size, MEM_COMMIT, win_prot);
    } else if (target_vaddr) {
        // Fallback: commit regular virtual pages if view mapping failed
        mapped = VirtualAlloc(target_vaddr, size, MEM_COMMIT, win_prot);
    }

    return mapped;
}

bool VirtualMemoryManager::UnmapDirectMemory(void* vaddr, u64 size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!vaddr) return false;
    if (UnmapViewOfFile(vaddr)) {
        return true;
    }
    return VirtualFree(vaddr, size, MEM_DECOMMIT) != FALSE;
}

void* VirtualMemoryManager::AllocateFlexibleMemory(u64 size, u32 protection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    DWORD win_prot = PAGE_READWRITE;
    if (protection & 1) win_prot = PAGE_EXECUTE_READWRITE;

    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, win_prot);
}

bool VirtualMemoryManager::FreeFlexibleMemory(void* vaddr, u64 size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!vaddr) return false;
    return VirtualFree(vaddr, 0, MEM_RELEASE) != FALSE;
}



u64 VirtualMemoryManager::GetAvailableDirectMemorySize() const {
    return TOTAL_DMEM_POOL_SIZE - m_dmem_allocated;
}

} // namespace Memory
