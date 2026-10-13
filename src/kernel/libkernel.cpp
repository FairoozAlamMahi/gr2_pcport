#include "kernel/libkernel.h"
#include "memory/vm.h"
#include "loader/elf_loader.h"
#include "common/logger.h"
#include <fcntl.h>
#include <io.h>
#include <vector>
#include <string>
#include <chrono>

namespace Kernel {

static std::string s_app0_root = ".";

void Initialize() {
    LOG_INFO("KERNEL", "libkernel emulation layer initialized.");
}

void Shutdown() {
    LOG_INFO("KERNEL", "libkernel emulation layer shut down.");
}

extern "C" {

PS4_SYSV_ABI s32 sceKernelAllocateDirectMemory(s64 search_start, s64 search_end, u64 length, u64 alignment, s32 type, s64* out_dest) {
    if (!out_dest) return -1;
    u64 offset = Memory::VirtualMemoryManager::Instance().AllocateDirectMemory(length, alignment);
    if (offset == ~0ULL) {
        return -1;
    }
    *out_dest = static_cast<s64>(offset);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMapDirectMemory(void** out_addr, u64 length, s32 protection, s32 flags, s64 dmem_offset, u64 alignment) {
    if (!out_addr) return -1;
    void* mapped = Memory::VirtualMemoryManager::Instance().MapDirectMemory(
        static_cast<u64>(dmem_offset), length, *out_addr, static_cast<u32>(protection));
    if (!mapped) return -1;
    *out_addr = mapped;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelReleaseDirectMemory(s64 dmem_offset, u64 length) {
    return Memory::VirtualMemoryManager::Instance().ReleaseDirectMemory(static_cast<u64>(dmem_offset), length) ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelGetDirectMemorySize(u64* out_size) {
    if (!out_size) return -1;
    *out_size = Memory::VirtualMemoryManager::Instance().GetTotalDirectMemorySize();
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMapFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags) {
    if (!out_addr) return -1;
    void* mapped = Memory::VirtualMemoryManager::Instance().AllocateFlexibleMemory(length, static_cast<u32>(protection));
    if (!mapped) return -1;
    *out_addr = mapped;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelOpen(const char* path, s32 flags, u32 mode) {
    if (!path) return -1;
    // Map /app0/ to game directory
    std::string resolved_path = path;
    if (resolved_path.rfind("/app0/", 0) == 0) {
        resolved_path = s_app0_root + "/" + resolved_path.substr(6);
    }

    int host_flags = _O_BINARY;
    if (flags & 0x0001) host_flags |= _O_WRONLY;
    else if (flags & 0x0002) host_flags |= _O_RDWR;
    else host_flags |= _O_RDONLY;

    if (flags & 0x0200) host_flags |= _O_CREAT;
    if (flags & 0x0400) host_flags |= _O_TRUNC;

    int fd = _open(resolved_path.c_str(), host_flags, _S_IREAD | _S_IWRITE);
    return fd;
}

PS4_SYSV_ABI s64 sceKernelRead(s32 fd, void* buf, u64 nbytes) {
    if (fd < 0 || !buf) return -1;
    int res = _read(fd, buf, static_cast<unsigned int>(nbytes));
    return static_cast<s64>(res);
}

PS4_SYSV_ABI s64 sceKernelWrite(s32 fd, const void* buf, u64 nbytes) {
    if (fd < 0 || !buf) return -1;
    int res = _write(fd, buf, static_cast<unsigned int>(nbytes));
    return static_cast<s64>(res);
}

PS4_SYSV_ABI s64 sceKernelLseek(s32 fd, s64 offset, s32 whence) {
    if (fd < 0) return -1;
    return _lseeki64(fd, offset, whence);
}

PS4_SYSV_ABI s32 sceKernelClose(s32 fd) {
    if (fd < 0) return -1;
    return _close(fd);
}

struct ThreadWrapperArgs {
    PthreadEntry entry;
    void* arg;
};

static DWORD WINAPI GuestThreadStub(LPVOID param) {
    auto* args = reinterpret_cast<ThreadWrapperArgs*>(param);
    PthreadEntry entry = args->entry;
    void* arg = args->arg;
    delete args;

    // Set up TCB for this thread in Windows TEB
    Loader::ElfLoader::Instance().SetupThreadTCB();

    // Execute System V ABI guest thread entry
    entry(arg);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCreate(void** thread, const void* attr, PthreadEntry entry, void* arg, const char* name) {
    if (!thread || !entry) return -1;

    auto* wrapper = new ThreadWrapperArgs{ entry, arg };

    // Create thread with fully committed 2 MB stack to prevent guard-page faults
    DWORD thread_id = 0;
    HANDLE hThread = CreateThread(
        nullptr,
        2 * 1024 * 1024, // 2 MB stack
        GuestThreadStub,
        wrapper,
        STACK_SIZE_PARAM_IS_A_RESERVATION,
        &thread_id
    );

    if (!hThread) {
        delete wrapper;
        return -1;
    }

    *thread = reinterpret_cast<void*>(hThread);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadJoin(void* thread, void** value_ptr) {
    if (!thread) return -1;
    HANDLE hThread = reinterpret_cast<HANDLE>(thread);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    return 0;
}

PS4_SYSV_ABI void* scePthreadSelf() {
    return reinterpret_cast<void*>(GetCurrentThread());
}

PS4_SYSV_ABI void scePthreadExit(void* value_ptr) {
    ExitThread(0);
}

PS4_SYSV_ABI s32 sceKernelCreateSema(void** sema, const char* name, u32 attr, s32 init_val, s32 max_val, const void* opt) {
    if (!sema) return -1;
    HANDLE hSema = CreateSemaphoreW(nullptr, init_val, max_val, nullptr);
    if (!hSema) return -1;
    *sema = reinterpret_cast<void*>(hSema);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelWaitSema(void* sema, s32 need, void* timeout) {
    if (!sema) return -1;
    DWORD wait_ms = timeout ? 1000 : INFINITE;
    DWORD res = WaitForSingleObject(reinterpret_cast<HANDLE>(sema), wait_ms);
    return (res == WAIT_OBJECT_0) ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelSignalSema(void* sema, s32 count) {
    if (!sema) return -1;
    return ReleaseSemaphore(reinterpret_cast<HANDLE>(sema), count, nullptr) ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelDeleteSema(void* sema) {
    if (!sema) return -1;
    CloseHandle(reinterpret_cast<HANDLE>(sema));
    return 0;
}

PS4_SYSV_ABI u64 sceKernelGetProcessTime() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
}

PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<u64>(counter.QuadPart);
}

PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounterFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return static_cast<u64>(freq.QuadPart);
}

} // extern "C"

} // namespace Kernel
