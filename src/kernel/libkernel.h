#pragma once

#include "common/types.h"
#include <windows.h>

namespace Kernel {

// Initialization of host POSIX and kernel virtualization
void Initialize();
void Shutdown();

// Exported libkernel functions (marked with PS4_SYSV_ABI)
extern "C" {

// Direct Memory
PS4_SYSV_ABI s32 sceKernelAllocateDirectMemory(s64 search_start, s64 search_end, u64 length, u64 alignment, s32 type, s64* out_dest);
PS4_SYSV_ABI s32 sceKernelMapDirectMemory(void** out_addr, u64 length, s32 protection, s32 flags, s64 dmem_offset, u64 alignment);
PS4_SYSV_ABI s32 sceKernelReleaseDirectMemory(s64 dmem_offset, u64 length);
PS4_SYSV_ABI s32 sceKernelGetDirectMemorySize(u64* out_size);
PS4_SYSV_ABI s32 sceKernelMapFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags);

// POSIX File I/O
PS4_SYSV_ABI s32 sceKernelOpen(const char* path, s32 flags, u32 mode);
PS4_SYSV_ABI s64 sceKernelRead(s32 fd, void* buf, u64 nbytes);
PS4_SYSV_ABI s64 sceKernelWrite(s32 fd, const void* buf, u64 nbytes);
PS4_SYSV_ABI s64 sceKernelLseek(s32 fd, s64 offset, s32 whence);
PS4_SYSV_ABI s32 sceKernelClose(s32 fd);

// POSIX Threading
using PthreadEntry = void* (PS4_SYSV_ABI *)(void*);
PS4_SYSV_ABI s32 scePthreadCreate(void** thread, const void* attr, PthreadEntry entry, void* arg, const char* name);
PS4_SYSV_ABI s32 scePthreadJoin(void* thread, void** value_ptr);
PS4_SYSV_ABI void* scePthreadSelf();
PS4_SYSV_ABI void scePthreadExit(void* value_ptr);

// Semaphores
PS4_SYSV_ABI s32 sceKernelCreateSema(void** sema, const char* name, u32 attr, s32 init_val, s32 max_val, const void* opt);
PS4_SYSV_ABI s32 sceKernelWaitSema(void* sema, s32 need, void* timeout);
PS4_SYSV_ABI s32 sceKernelSignalSema(void* sema, s32 count);
PS4_SYSV_ABI s32 sceKernelDeleteSema(void* sema);

// Misc
PS4_SYSV_ABI u64 sceKernelGetProcessTime();
PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounter();
PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounterFrequency();

} // extern "C"

} // namespace Kernel
