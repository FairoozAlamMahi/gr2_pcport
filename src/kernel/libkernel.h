#pragma once

#include "common/types.h"
#include <windows.h>

namespace Kernel {

// Initialization of host POSIX and kernel virtualization
void Initialize();
void Shutdown();

// Exported libkernel functions (marked with PS4_SYSV_ABI)
extern "C" {

// Direct & Flexible Memory
PS4_SYSV_ABI s32 sceKernelAllocateDirectMemory(s64 search_start, s64 search_end, u64 length, u64 alignment, s32 type, s64* out_dest);
PS4_SYSV_ABI s32 sceKernelMapDirectMemory(void** out_addr, u64 length, s32 protection, s32 flags, s64 dmem_offset, u64 alignment);
PS4_SYSV_ABI s32 sceKernelReleaseDirectMemory(s64 dmem_offset, u64 length);
PS4_SYSV_ABI u64 sceKernelGetDirectMemorySize();
PS4_SYSV_ABI u64 sceKernelAvailableFlexibleMemorySize(u64* out_size);
PS4_SYSV_ABI s32 sceKernelMapFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags);
PS4_SYSV_ABI s32 sceKernelMapNamedFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags, const char* name);
PS4_SYSV_ABI s32 sceKernelReleaseFlexibleMemory(void* addr, u64 length);
PS4_SYSV_ABI s32 sceKernelIsNeoMode();

// POSIX Memory Mapping (mmap)
PS4_SYSV_ABI void* mmap(void* addr, u64 length, s32 prot, s32 flags, s32 fd, s64 offset);
PS4_SYSV_ABI s32 munmap(void* addr, u64 length);
PS4_SYSV_ABI s32 sceKernelMunmap(void* addr, u64 length);
PS4_SYSV_ABI s32 msync(void* addr, u64 length, s32 flags);
PS4_SYSV_ABI s32 getpagesize();
PS4_SYSV_ABI s32 sceKernelMlock(const void* addr, u64 len);
PS4_SYSV_ABI s32 _sceKernelRtldSetApplicationHeapAPI(void* api);

// POSIX File I/O
PS4_SYSV_ABI s32 sceKernelOpen(const char* path, s32 flags, u32 mode);
PS4_SYSV_ABI s64 sceKernelRead(s32 fd, void* buf, u64 nbytes);
PS4_SYSV_ABI s64 sceKernelWrite(s32 fd, const void* buf, u64 nbytes);
PS4_SYSV_ABI s64 sceKernelLseek(s32 fd, s64 offset, s32 whence);
PS4_SYSV_ABI s32 sceKernelClose(s32 fd);
PS4_SYSV_ABI s64 sceKernelPread(s32 fd, void* buf, u64 nbytes, s64 offset);
PS4_SYSV_ABI s64 sceKernelPwrite(s32 fd, const void* buf, u64 nbytes, s64 offset);
PS4_SYSV_ABI s64 ps4_read(s32 fd, void* buf, u64 nbytes);
PS4_SYSV_ABI s64 ps4_write(s32 fd, const void* buf, u64 nbytes);
PS4_SYSV_ABI s32 sceKernelFstat(s32 fd, void* sb);
PS4_SYSV_ABI s32 sceKernelStat(const char* path, void* sb);
PS4_SYSV_ABI s32 sceKernelFsync(s32 fd);
PS4_SYSV_ABI s32 sceKernelFtruncate(s32 fd, s64 length);
PS4_SYSV_ABI s32 sceKernelTruncate(const char* path, s64 length);
PS4_SYSV_ABI s32 sceKernelMkdir(const char* path, u32 mode);
PS4_SYSV_ABI s32 sceKernelUnlink(const char* path);
PS4_SYSV_ABI s32 sceKernelRename(const char* oldp, const char* newp);
PS4_SYSV_ABI s32 sceKernelRmdir(const char* path);
PS4_SYSV_ABI s32 sceKernelGetdirentries(s32 fd, char* buf, u32 nbytes, s64* basep);
PS4_SYSV_ABI s32 sceKernelGetdents(s32 fd, char* buf, u32 nbytes);

// POSIX Threading
using PthreadEntry = void* (PS4_SYSV_ABI *)(void*);
PS4_SYSV_ABI s32 scePthreadCreate(void** thread, const void* attr, PthreadEntry entry, void* arg, const char* name);
PS4_SYSV_ABI s32 scePthreadJoin(void* thread, void** value_ptr);
PS4_SYSV_ABI void* scePthreadSelf();
PS4_SYSV_ABI void scePthreadExit(void* value_ptr);
PS4_SYSV_ABI s32 scePthreadDetach(void* thread);
PS4_SYSV_ABI s32 scePthreadEqual(void* t1, void* t2);
PS4_SYSV_ABI void scePthreadYield();
PS4_SYSV_ABI s32 scePthreadSetaffinity(void* thread, u64 mask);
PS4_SYSV_ABI s32 scePthreadGetaffinity(void* thread, u64* mask);
PS4_SYSV_ABI s32 scePthreadSetprio(void* thread, s32 prio);
PS4_SYSV_ABI s32 scePthreadGetprio(void* thread, s32* prio);
PS4_SYSV_ABI s32 scePthreadGetthreadid();

// Pthread Keys & TLS
PS4_SYSV_ABI s32 scePthreadKeyCreate(u32* key, void (*destructor)(void*));
PS4_SYSV_ABI s32 scePthreadKeyDelete(u32 key);
PS4_SYSV_ABI void* scePthreadGetspecific(u32 key);
PS4_SYSV_ABI s32 scePthreadSetspecific(u32 key, const void* value);
PS4_SYSV_ABI s32 scePthreadOnce(s32* once_control, void (*init_routine)());

// Pthread Attributes
PS4_SYSV_ABI s32 scePthreadAttrInit(void* attr);
PS4_SYSV_ABI s32 scePthreadAttrDestroy(void* attr);
PS4_SYSV_ABI s32 scePthreadAttrSetstacksize(void* attr, size_t stacksize);
PS4_SYSV_ABI s32 scePthreadAttrGetstacksize(const void* attr, size_t* stacksize);
PS4_SYSV_ABI s32 scePthreadAttrSetdetachstate(void* attr, s32 state);
PS4_SYSV_ABI s32 scePthreadAttrSetschedpolicy(void* attr, s32 policy);
PS4_SYSV_ABI s32 scePthreadAttrSetschedparam(void* attr, const void* param);
PS4_SYSV_ABI s32 scePthreadAttrSetaffinity(void* attr, u64 mask);
PS4_SYSV_ABI s32 scePthreadAttrGetaffinity(const void* attr, u64* mask);
PS4_SYSV_ABI s32 scePthreadAttrSetinheritsched(void* attr, s32 inherit);
PS4_SYSV_ABI s32 scePthreadAttrSetguardsize(void* attr, size_t guardsize);
PS4_SYSV_ABI s32 scePthreadAttrGet(void* thread, void* attr);

// Pthread Mutexes
PS4_SYSV_ABI s32 scePthreadMutexattrInit(void* attr);
PS4_SYSV_ABI s32 scePthreadMutexattrSettype(void* attr, s32 type);
PS4_SYSV_ABI s32 scePthreadMutexattrSetprotocol(void* attr, s32 protocol);
PS4_SYSV_ABI s32 scePthreadMutexattrDestroy(void* attr);
PS4_SYSV_ABI s32 pthread_mutex_init(void** mutex, const void* attr);
PS4_SYSV_ABI s32 scePthreadMutexInit(void** mutex, const void* attr, const char* name);
PS4_SYSV_ABI s32 scePthreadMutexLock(void** mutex);
PS4_SYSV_ABI s32 scePthreadMutexTrylock(void** mutex);
PS4_SYSV_ABI s32 scePthreadMutexUnlock(void** mutex);
PS4_SYSV_ABI s32 scePthreadMutexDestroy(void** mutex);

// Pthread Condition Variables & POSIX Time
struct OrbisTimespec {
    s64 tv_sec;
    s64 tv_nsec;
};

// FreeBSD 9 / Orbis OS struct stat (120 bytes)
struct OrbisStat {
    u32 st_dev;           // 0..4
    u32 st_ino;           // 4..8
    u16 st_mode;          // 8..10
    u16 st_nlink;         // 10..12
    u32 st_uid;           // 12..16
    u32 st_gid;           // 16..20
    u32 st_rdev;          // 20..24
    OrbisTimespec st_atim;// 24..40
    OrbisTimespec st_mtim;// 40..56
    OrbisTimespec st_ctim;// 56..72
    s64 st_size;          // 72..80
    s64 st_blocks;        // 80..88
    u32 st_blksize;       // 88..92
    u32 st_flags;         // 92..96
    u32 st_gen;           // 96..100
    s32 st_lspare;        // 100..104
    OrbisTimespec st_birthtim; // 104..120
};

// FreeBSD 9 / Orbis OS struct dirent (standard directory entry)
#pragma pack(push, 1)
struct OrbisDirent {
    u32 d_fileno;         // 0..4 (inode)
    u16 d_reclen;         // 4..6 (record length)
    u8  d_type;           // 6..7 (file type: DT_DIR=4, DT_REG=8)
    u8  d_namlen;         // 7..8 (name length)
    char d_name[256];     // 8..264 (filename)
};
#pragma pack(pop)

extern uintptr_t g_stack_chk_guard;
extern uintptr_t g_stdout_obj;

PS4_SYSV_ABI s32 scePthreadCondattrInit(void* attr);
PS4_SYSV_ABI s32 scePthreadCondattrDestroy(void* attr);
PS4_SYSV_ABI s32 pthread_cond_init(void** cond, const void* attr);
PS4_SYSV_ABI s32 scePthreadCondInit(void** cond, const void* attr, const char* name);
PS4_SYSV_ABI s32 scePthreadCondDestroy(void** cond);
PS4_SYSV_ABI s32 scePthreadCondSignal(void** cond);
PS4_SYSV_ABI s32 scePthreadCondBroadcast(void** cond);
PS4_SYSV_ABI s32 scePthreadCondWait(void** cond, void** mutex);
PS4_SYSV_ABI s32 scePthreadCondTimedwait(void** cond, void** mutex, u64 usec);
PS4_SYSV_ABI s32 pthread_cond_timedwait(void** cond, void** mutex, const OrbisTimespec* abstime);

// Pthread Read-Write Locks
PS4_SYSV_ABI s32 scePthreadRwlockattrInit(void* attr);
PS4_SYSV_ABI s32 scePthreadRwlockattrDestroy(void* attr);
PS4_SYSV_ABI s32 scePthreadRwlockInit(void** rwlock, const void* attr, const char* name);
PS4_SYSV_ABI s32 scePthreadRwlockDestroy(void** rwlock);
PS4_SYSV_ABI s32 scePthreadRwlockRdlock(void** rwlock);
PS4_SYSV_ABI s32 scePthreadRwlockTryrdlock(void** rwlock);
PS4_SYSV_ABI s32 scePthreadRwlockWrlock(void** rwlock);
PS4_SYSV_ABI s32 scePthreadRwlockTrywrlock(void** rwlock);
PS4_SYSV_ABI s32 scePthreadRwlockUnlock(void** rwlock);

// Semaphores
PS4_SYSV_ABI s32 sceKernelCreateSema(void** sema, const char* name, u32 attr, s32 init_val, s32 max_val, const void* opt);
PS4_SYSV_ABI s32 sceKernelWaitSema(void* sema, s32 need, void* timeout);
PS4_SYSV_ABI s32 sceKernelPollSema(void* sema, s32 need);
PS4_SYSV_ABI s32 sceKernelSignalSema(void* sema, s32 count);
PS4_SYSV_ABI s32 sceKernelDeleteSema(void* sema);

// Event Flags
PS4_SYSV_ABI s32 sceKernelCreateEventFlag(void** flag, const char* name, u32 attr, u64 init_pattern, const void* opt);
PS4_SYSV_ABI s32 sceKernelDeleteEventFlag(void* flag);
PS4_SYSV_ABI s32 sceKernelSetEventFlag(void* flag, u64 pattern);
PS4_SYSV_ABI s32 sceKernelClearEventFlag(void* flag, u64 pattern);
PS4_SYSV_ABI s32 sceKernelWaitEventFlag(void* flag, u64 bits, u32 wait_mode, u64* out_bits, void* timeout);
PS4_SYSV_ABI s32 sceKernelPollEventFlag(void* flag, u64 bits, u32 wait_mode, u64* out_bits);
PS4_SYSV_ABI s32 sceKernelCancelEventFlag(void* flag, u64 pattern, s32* num_threads);

// Event Queues
PS4_SYSV_ABI s32 sceKernelCreateEqueue(void** eq, const char* name);
PS4_SYSV_ABI s32 sceKernelDeleteEqueue(void* eq);
PS4_SYSV_ABI s32 sceKernelWaitEqueue(void* eq, void* events, s32 num_events, s32* out_events, void* timeout);
PS4_SYSV_ABI s32 sceKernelAddUserEvent(void* eq, s32 id);
PS4_SYSV_ABI s32 sceKernelAddReadEvent(void* eq, s32 fd, void* udata);
PS4_SYSV_ABI s32 sceKernelAddWriteEvent(void* eq, s32 fd, void* udata);
PS4_SYSV_ABI s32 sceKernelAddTimerEvent(void* eq, s32 id, u64 time, void* udata);
PS4_SYSV_ABI s32 sceKernelDeleteUserEvent(void* eq, s32 id);
PS4_SYSV_ABI s32 sceKernelDeleteReadEvent(void* eq, s32 fd);
PS4_SYSV_ABI s32 sceKernelDeleteWriteEvent(void* eq, s32 fd);
PS4_SYSV_ABI s32 sceKernelDeleteTimerEvent(void* eq, s32 id);
PS4_SYSV_ABI s32 sceKernelTriggerUserEvent(void* eq, s32 id, void* udata);
PS4_SYSV_ABI s32 sceKernelGetEventFilter(void* event);
PS4_SYSV_ABI s32 sceKernelGetEventId(void* event);

// Time & Timing
PS4_SYSV_ABI s32 sceKernelClockGettime(s32 clock_id, OrbisTimespec* tp);
PS4_SYSV_ABI s32 sceKernelGettimeofday(void* tv, void* tz);
PS4_SYSV_ABI u64 sceKernelGetProcessTime();
PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounter();
PS4_SYSV_ABI u64 sceKernelGetProcessTimeCounterFrequency();
PS4_SYSV_ABI u64 sceKernelGetTscFrequency();
PS4_SYSV_ABI u64 sceKernelReadTsc();
PS4_SYSV_ABI s32 sceKernelUsleep(u32 useconds);
PS4_SYSV_ABI s32 sceKernelSleep(u32 seconds);
PS4_SYSV_ABI s32 ps4_nanosleep(const OrbisTimespec* req, OrbisTimespec* rem);

// Modules & System Services
PS4_SYSV_ABI s32 sceSysmoduleLoadModule(u32 id);
PS4_SYSV_ABI s32 sceSysmoduleUnloadModule(u32 id);
PS4_SYSV_ABI s32 sceSysmoduleIsLoaded(u32 id);
PS4_SYSV_ABI s32 sceKernelLoadStartModule(const char* name, u64 argc, const void* argv, u32 flags, const void* opt, s32* res);
PS4_SYSV_ABI s32 sceKernelStopUnloadModule(s32 id, u64 argc, const void* argv, u32 flags, const void* opt, s32* res);
PS4_SYSV_ABI s32 sceKernelGetModuleInfoFromAddr(const void* addr, u32 flags, void* info);
PS4_SYSV_ABI s32 sceKernelGetModuleInfoForUnwind(const void* addr, void* info);
PS4_SYSV_ABI s32 sceKernelInternalMemoryGetModuleSegmentInfo(void* info);
PS4_SYSV_ABI s32 sceKernelGetProcParam(void* param);

// Miscellaneous & CRT
PS4_SYSV_ABI void* ps4_malloc(u64 size);
PS4_SYSV_ABI void ps4_free(void* ptr);
PS4_SYSV_ABI void* ps4_calloc(u64 num, u64 size);
PS4_SYSV_ABI void* ps4_realloc(void* ptr, u64 size);
PS4_SYSV_ABI void* ps4_memalign(u64 alignment, u64 size);
PS4_SYSV_ABI s32 ps4_posix_memalign(void** memptr, u64 alignment, u64 size);
PS4_SYSV_ABI int* __error();
PS4_SYSV_ABI void __stack_chk_fail();
PS4_SYSV_ABI void* __tls_get_addr(void* ti);
PS4_SYSV_ABI void ps4_exit(s32 status);
PS4_SYSV_ABI s32 sceKernelGetCurrentCpu();
PS4_SYSV_ABI s32 sceKernelIsStack(const void* addr);
PS4_SYSV_ABI s32 sceKernelSetGPO(u32 gpo);
PS4_SYSV_ABI s32 sceKernelUuidCreate(void* uuid);
PS4_SYSV_ABI s32 sceKernelConvertUtcToLocaltime(s64 utc, s64* local);
PS4_SYSV_ABI s32 sceKernelConvertLocaltimeToUtc(s64 local, s64* utc);
PS4_SYSV_ABI void sceKernelDebugRaiseException(s32 code);
PS4_SYSV_ABI void sceKernelPrintBacktraceWithModuleInfo();

// Sockets & Network POSIX
PS4_SYSV_ABI s32 ps4_socket(s32 domain, s32 type, s32 protocol);
PS4_SYSV_ABI s32 ps4_bind(s32 sockfd, const void* addr, u32 addrlen);
PS4_SYSV_ABI s32 ps4_listen(s32 sockfd, s32 backlog);
PS4_SYSV_ABI s32 ps4_accept(s32 sockfd, void* addr, u32* addrlen);
PS4_SYSV_ABI s64 ps4_send(s32 sockfd, const void* buf, u64 len, s32 flags);
PS4_SYSV_ABI s64 ps4_recv(s32 sockfd, void* buf, u64 len, s32 flags);
PS4_SYSV_ABI s32 ps4_shutdown(s32 sockfd, s32 how);
PS4_SYSV_ABI s32 ps4_select(s32 nfds, void* readfds, void* writefds, void* exceptfds, void* timeout);
PS4_SYSV_ABI s32 getrusage(s32 who, void* usage);

// Signals & Exception Handlers
PS4_SYSV_ABI s32 sigprocmask(s32 how, const void* set, void* oset);
PS4_SYSV_ABI s32 _sigprocmask(s32 how, const void* set, void* oset);
PS4_SYSV_ABI s32 sigfillset(void* set);
PS4_SYSV_ABI void* ps4_signal(s32 signum, void* handler);
PS4_SYSV_ABI s32 sigreturn(void* ucp);
PS4_SYSV_ABI s32 _is_signal_return();
PS4_SYSV_ABI void __pthread_cxa_finalize(void* dso);
PS4_SYSV_ABI s32 __elf_phdr_match_addr(void* addr, void* info);

// System Service, Content, DiscMap & Display
PS4_SYSV_ABI s32 sceDiscMapIsRequestOnHDD(void* request);
PS4_SYSV_ABI s64 sceDiscMapGetPackageSize(void* p);
PS4_SYSV_ABI s32 ps4_disc_map_get_location(void* a, u64 b, void* c, u64* out_location, u64* out_lba, u64* out_size);
PS4_SYSV_ABI s32 sceVideoOutGetResolutionStatus(s32 handle, void* status);
PS4_SYSV_ABI s32 sceSystemServiceParamGetInt(s32 param_id, s32* out_val);
PS4_SYSV_ABI s32 sceSystemServiceGetStatus(void* status);
PS4_SYSV_ABI s32 sceSystemServiceReceiveEvent(void* event);
PS4_SYSV_ABI s32 sceAppContentAppParamGetInt(s32 param_id, s32* out_val);
PS4_SYSV_ABI s32 sceAppContentGetAddcontInfoList(s32 a, s32 b, s32 c, u32* out_count);
PS4_SYSV_ABI s32 sceUserServiceGetEvent(void* event);
PS4_SYSV_ABI s32 sceUserServiceInitialize(const void* param);
PS4_SYSV_ABI s32 sceUserServiceTerminate();
PS4_SYSV_ABI s32 sceUserServiceGetInitialUser(s32* user_id);
PS4_SYSV_ABI s32 sceUserServiceGetLoginUserIdList(s32* user_ids);
PS4_SYSV_ABI s32 sceUserServiceGetUserName(s32 user_id, char* name, usize size);

// RTC (Real Time Clock)
PS4_SYSV_ABI s32 sceRtcGetCurrentNetworkTick(void* tick_out);
PS4_SYSV_ABI s32 sceRtcGetTick(const void* time, void* tick_out);

// PlayGo Background Streaming
PS4_SYSV_ABI s32 scePlayGoInitialize(const void* param);
PS4_SYSV_ABI s32 scePlayGoOpen(s32* handle, const void* param);
PS4_SYSV_ABI s32 scePlayGoClose(s32 handle);
PS4_SYSV_ABI s32 scePlayGoTerminate();
PS4_SYSV_ABI s32 scePlayGoGetLocus(s32 handle, const u32* chunk_ids, u32 num_chunks, void* locus_array);
PS4_SYSV_ABI s32 scePlayGoGetProgress(s32 handle, const u32* chunk_ids, u32 num_chunks, void* progress_array);

// Dialogs & System Services
PS4_SYSV_ABI s32 sceSystemServiceHideSplashScreen();
PS4_SYSV_ABI s32 sceAppContentInitialize(const void* param);
PS4_SYSV_ABI s32 sceShareUtilityInitialize(const void* param);
PS4_SYSV_ABI s32 sceNpSetContentRestriction(const void* param);

} // extern "C"

} // namespace Kernel
