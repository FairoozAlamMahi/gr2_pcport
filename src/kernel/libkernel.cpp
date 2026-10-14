#include "kernel/libkernel.h"
#include "memory/vm.h"
#include "loader/elf_loader.h"
#include "common/logger.h"
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <direct.h>
#include <vector>
#include <string>
#include <chrono>
#include <intrin.h>
#include <rpc.h>
#include <filesystem>

namespace Kernel {

namespace fs = std::filesystem;
static std::string s_app0_root = "../gr2_packages/merged_game";

void Initialize() {
    if (fs::exists("../gr2_packages/merged_game")) {
        s_app0_root = "../gr2_packages/merged_game";
    } else if (fs::exists("game/Gravite2_Chunk0.psarc")) {
        s_app0_root = "game";
    } else {
        s_app0_root = ".";
    }
    LOG_INFO("KERNEL", "libkernel emulation layer initialized with full POSIX & synchronization subsystem.");
    LOG_INFO("KERNEL", "libkernel filesystem app0 root configured: " + s_app0_root);
}

static std::string ResolveGuestPath(const char* path) {
    if (!path) return "";
    std::string p = path;
    if (p.rfind("/app0/", 0) == 0) {
        p = s_app0_root + "/" + p.substr(6);
    } else if (p.rfind("/app0", 0) == 0) {
        p = s_app0_root + p.substr(5);
    } else if (p.rfind("/arc0/", 0) == 0) {
        std::string sub = p.substr(6);
        if (fs::exists(s_app0_root + "/arc0/" + sub)) {
            p = s_app0_root + "/arc0/" + sub;
        } else if (fs::exists("../gr2_packages/extracted_arc0/" + sub)) {
            p = "../gr2_packages/extracted_arc0/" + sub;
        } else if (fs::exists("game/arc0/" + sub)) {
            p = "game/arc0/" + sub;
        }
    } else if (p.rfind("/dev/zero", 0) == 0) {
        p = "NUL";
    }
    return p;
}

struct DirEntryInfo {
    std::string name;
    bool is_dir;
};

struct DirectoryState {
    std::string path;
    std::vector<DirEntryInfo> entries;
    size_t index = 0;
};

static std::mutex s_dirs_mutex;
static std::unordered_map<s32, DirectoryState> s_open_dirs;

void Shutdown() {
    LOG_INFO("KERNEL", "libkernel emulation layer shut down.");
}

extern "C" {

// Direct & Flexible Memory
PS4_SYSV_ABI s32 sceKernelAllocateDirectMemory(s64 search_start, s64 search_end, u64 length, u64 alignment, s32 type, s64* out_dest) {
    if (!out_dest) return -1;
    u64 offset = Memory::VirtualMemoryManager::Instance().AllocateDirectMemory(length, alignment);
    if (offset == ~0ULL) {
        LOG_ERROR("KERNEL", "sceKernelAllocateDirectMemory failed for length: " + std::to_string(length));
        return -1;
    }
    *out_dest = static_cast<s64>(offset);
    LOG_INFO("KERNEL", "sceKernelAllocateDirectMemory: length=0x" + std::to_string(length) + 
             " -> offset=0x" + std::to_string(offset));
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMapDirectMemory(void** out_addr, u64 length, s32 protection, s32 flags, s64 dmem_offset, u64 alignment) {
    if (!out_addr) return -1;
    void* mapped = Memory::VirtualMemoryManager::Instance().MapDirectMemory(
        static_cast<u64>(dmem_offset), length, *out_addr, static_cast<u32>(protection));
    if (!mapped) {
        LOG_ERROR("KERNEL", "sceKernelMapDirectMemory failed!");
        return -1;
    }
    *out_addr = mapped;
    LOG_INFO("KERNEL", "sceKernelMapDirectMemory: offset=0x" + std::to_string(dmem_offset) + 
             " length=0x" + std::to_string(length) + " -> addr=0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(mapped)));
    return 0;
}

PS4_SYSV_ABI s32 sceKernelReleaseDirectMemory(s64 dmem_offset, u64 length) {
    return Memory::VirtualMemoryManager::Instance().ReleaseDirectMemory(static_cast<u64>(dmem_offset), length) ? 0 : -1;
}

PS4_SYSV_ABI u64 sceKernelGetDirectMemorySize() {
    u64 sz = Memory::VirtualMemoryManager::Instance().GetTotalDirectMemorySize();
    LOG_INFO("KERNEL", "sceKernelGetDirectMemorySize() -> 0x" + std::to_string(sz));
    return sz;
}

PS4_SYSV_ABI u64 sceKernelAvailableFlexibleMemorySize(u64* out_size) {
    constexpr u64 FLEXIBLE_MEM_SIZE = 0x80000000ULL; // 2048 MB
    if (out_size) {
        *out_size = FLEXIBLE_MEM_SIZE;
    }
    LOG_INFO("KERNEL", "sceKernelAvailableFlexibleMemorySize() -> 0x" + std::to_string(FLEXIBLE_MEM_SIZE));
    return FLEXIBLE_MEM_SIZE;
}

PS4_SYSV_ABI s32 sceKernelMapFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags) {
    if (!out_addr) return -1;
    void* target = *out_addr;
    DWORD win_prot = (protection & 1) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    void* mapped = nullptr;
    if (target) {
        mapped = VirtualAlloc(target, length, MEM_COMMIT, win_prot);
        if (!mapped) {
            mapped = VirtualAlloc(target, length, MEM_RESERVE | MEM_COMMIT, win_prot);
        }
    }
    if (!mapped) {
        mapped = VirtualAlloc(nullptr, length, MEM_RESERVE | MEM_COMMIT, win_prot);
    }
    if (!mapped) {
        LOG_ERROR("KERNEL", "sceKernelMapFlexibleMemory: VirtualAlloc FAILED for length: 0x" + 
                  std::to_string(length) + " Error: " + std::to_string(GetLastError()));
        return -1;
    }
    *out_addr = mapped;
    LOG_INFO("KERNEL", "sceKernelMapFlexibleMemory: length=0x" + std::to_string(length) + 
             " -> addr=0x" + std::to_string(reinterpret_cast<uintptr_t>(mapped)));
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMapNamedFlexibleMemory(void** out_addr, u64 length, s32 protection, s32 flags, const char* name) {
    return sceKernelMapFlexibleMemory(out_addr, length, protection, flags);
}

PS4_SYSV_ABI s32 sceKernelReleaseFlexibleMemory(void* addr, u64 length) {
    if (addr) {
        VirtualFree(addr, 0, MEM_RELEASE);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceKernelIsNeoMode() {
    return 1; // PS4 Pro / Neo Mode
}

// POSIX Memory Mapping (mmap)
PS4_SYSV_ABI void* mmap(void* addr, u64 length, s32 prot, s32 flags, s32 fd, s64 offset) {
    DWORD win_prot = (prot & 4) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    void* res = nullptr;
    if (addr) {
        res = VirtualAlloc(addr, length, MEM_COMMIT, win_prot);
        if (!res) res = VirtualAlloc(addr, length, MEM_RESERVE | MEM_COMMIT, win_prot);
    }
    if (!res) {
        res = VirtualAlloc(nullptr, length, MEM_RESERVE | MEM_COMMIT, win_prot);
    }
    return res ? res : reinterpret_cast<void*>(-1);
}

PS4_SYSV_ABI s32 munmap(void* addr, u64 length) {
    if (addr && addr != reinterpret_cast<void*>(-1)) {
        VirtualFree(addr, 0, MEM_RELEASE);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMunmap(void* addr, u64 length) {
    return munmap(addr, length);
}

PS4_SYSV_ABI s32 msync(void* addr, u64 length, s32 flags) {
    return 0;
}

PS4_SYSV_ABI s32 getpagesize() {
    return 4096;
}

PS4_SYSV_ABI s32 sceKernelMlock(const void* addr, u64 len) {
    return 0;
}

PS4_SYSV_ABI s32 _sceKernelRtldSetApplicationHeapAPI(void* api) {
    return 0;
}

// POSIX File I/O
PS4_SYSV_ABI s32 sceKernelOpen(const char* path, s32 flags, u32 mode) {
    if (!path) return -1;
    std::string resolved_path = ResolveGuestPath(path);

    int host_flags = _O_BINARY;
    if (flags & 0x0001) host_flags |= _O_WRONLY;
    else if (flags & 0x0002) host_flags |= _O_RDWR;
    else host_flags |= _O_RDONLY;

    if (flags & 0x0200) host_flags |= _O_CREAT;
    if (flags & 0x0400) host_flags |= _O_TRUNC;

    int fd = _open(resolved_path.c_str(), host_flags, _S_IREAD | _S_IWRITE);
    DWORD dwAttrs = GetFileAttributesA(resolved_path.c_str());
    if (fd < 0) {
        // Try opening as directory with backup semantics (FreeBSD allows opening directories)
        if (dwAttrs != INVALID_FILE_ATTRIBUTES && (dwAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            HANDLE hDir = CreateFileA(resolved_path.c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (hDir != INVALID_HANDLE_VALUE) {
                fd = _open_osfhandle(reinterpret_cast<intptr_t>(hDir), _O_RDONLY);
            }
        }
    }

    if (fd >= 0 && dwAttrs != INVALID_FILE_ATTRIBUTES && (dwAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::lock_guard<std::mutex> lock(s_dirs_mutex);
        DirectoryState state;
        state.path = resolved_path;
        state.entries.push_back({ ".", true });
        state.entries.push_back({ "..", true });
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(resolved_path, ec)) {
            state.entries.push_back({ entry.path().filename().string(), entry.is_directory(ec) });
        }
        size_t count = state.entries.size();
        s_open_dirs[fd] = std::move(state);
        LOG_INFO("KERNEL", "Directory opened: fd=" + std::to_string(fd) + " with " +
                 std::to_string(count) + " entries");
    }

    LOG_INFO("KERNEL", "sceKernelOpen: '" + std::string(path) + "' -> '" + resolved_path + 
             "' (fd=" + std::to_string(fd) + ")");
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
    {
        std::lock_guard<std::mutex> lock(s_dirs_mutex);
        s_open_dirs.erase(fd);
    }
    return _close(fd);
}

PS4_SYSV_ABI s64 sceKernelPread(s32 fd, void* buf, u64 nbytes, s64 offset) {
    if (fd < 0 || !buf) return -1;
    _lseeki64(fd, offset, SEEK_SET);
    int res = _read(fd, buf, static_cast<unsigned int>(nbytes));
    return static_cast<s64>(res);
}

PS4_SYSV_ABI s64 sceKernelPwrite(s32 fd, const void* buf, u64 nbytes, s64 offset) {
    if (fd < 0 || !buf) return -1;
    _lseeki64(fd, offset, SEEK_SET);
    int res = _write(fd, buf, static_cast<unsigned int>(nbytes));
    return static_cast<s64>(res);
}

PS4_SYSV_ABI s64 ps4_read(s32 fd, void* buf, u64 nbytes) {
    return sceKernelRead(fd, buf, nbytes);
}

PS4_SYSV_ABI s64 ps4_write(s32 fd, const void* buf, u64 nbytes) {
    return sceKernelWrite(fd, buf, nbytes);
}

PS4_SYSV_ABI s32 sceKernelFstat(s32 fd, void* sb) {
    if (fd < 0 || !sb) return -1;
    struct _stat64 st;
    if (_fstat64(fd, &st) != 0) return -1;
    auto* os = reinterpret_cast<OrbisStat*>(sb);
    std::memset(os, 0, sizeof(OrbisStat));
    if (st.st_mode & _S_IFDIR) {
        os->st_mode = 0x41ED; // S_IFDIR | 0755
    } else {
        os->st_mode = 0x81A4; // S_IFREG | 0644
    }
    os->st_size = st.st_size;
    os->st_blksize = 4096;
    os->st_blocks = (st.st_size + 511) / 512;
    os->st_nlink = 1;
    os->st_atim.tv_sec = st.st_atime;
    os->st_mtim.tv_sec = st.st_mtime;
    os->st_ctim.tv_sec = st.st_ctime;
    os->st_birthtim.tv_sec = st.st_ctime;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelStat(const char* path, void* sb) {
    if (!path || !sb) return -1;
    std::string p = ResolveGuestPath(path);
    struct _stat64 st;
    if (_stat64(p.c_str(), &st) != 0) {
        LOG_WARN("KERNEL", "sceKernelStat: file not found: '" + std::string(path) + "' (resolved: '" + p + "')");
        return -1;
    }
    auto* os = reinterpret_cast<OrbisStat*>(sb);
    std::memset(os, 0, sizeof(OrbisStat));
    if (st.st_mode & _S_IFDIR) {
        os->st_mode = 0x41ED; // S_IFDIR | 0755
    } else {
        os->st_mode = 0x81A4; // S_IFREG | 0644
    }
    os->st_size = st.st_size;
    os->st_blksize = 4096;
    os->st_blocks = (st.st_size + 511) / 512;
    os->st_nlink = 1;
    os->st_atim.tv_sec = st.st_atime;
    os->st_mtim.tv_sec = st.st_mtime;
    os->st_ctim.tv_sec = st.st_ctime;
    os->st_birthtim.tv_sec = st.st_ctime;
    LOG_INFO("KERNEL", "sceKernelStat: '" + std::string(path) + "' -> size=" + std::to_string(st.st_size) + 
             ", mode=0x" + std::to_string(os->st_mode));
    return 0;
}

PS4_SYSV_ABI s32 sceKernelFsync(s32 fd) {
    if (fd < 0) return -1;
    return _commit(fd);
}

PS4_SYSV_ABI s32 sceKernelFtruncate(s32 fd, s64 length) {
    if (fd < 0) return -1;
    return _chsize_s(fd, length);
}

PS4_SYSV_ABI s32 sceKernelTruncate(const char* path, s64 length) {
    return 0;
}

PS4_SYSV_ABI s32 sceKernelMkdir(const char* path, u32 mode) {
    if (!path) return -1;
    return CreateDirectoryA(path, nullptr) ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelUnlink(const char* path) {
    if (!path) return -1;
    return remove(path);
}

PS4_SYSV_ABI s32 sceKernelRename(const char* oldp, const char* newp) {
    if (!oldp || !newp) return -1;
    return rename(oldp, newp);
}

PS4_SYSV_ABI s32 sceKernelRmdir(const char* path) {
    if (!path) return -1;
    return RemoveDirectoryA(path) ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelGetdirentries(s32 fd, char* buf, u32 nbytes, s64* basep) {
    if (fd < 0 || !buf || nbytes == 0) return -1;
    std::lock_guard<std::mutex> lock(s_dirs_mutex);
    auto it = s_open_dirs.find(fd);
    if (it == s_open_dirs.end()) {
        LOG_WARN("KERNEL", "sceKernelGetdirentries: fd " + std::to_string(fd) + " not a tracked directory");
        return 0;
    }

    auto& dir = it->second;
    size_t start_idx = dir.index;
    if (basep && *basep >= 0 && static_cast<size_t>(*basep) <= dir.entries.size()) {
        start_idx = static_cast<size_t>(*basep);
    }

    if (start_idx >= dir.entries.size()) {
        LOG_INFO("KERNEL", "sceKernelGetdirentries: fd=" + std::to_string(fd) + " EOF reached (index " + 
                 std::to_string(start_idx) + "/" + std::to_string(dir.entries.size()) + ")");
        return 0;
    }

    u32 bytes_written = 0;
    size_t cur_idx = start_idx;

    while (cur_idx < dir.entries.size()) {
        const auto& item = dir.entries[cur_idx];
        u8 name_len = static_cast<u8>(std::min<size_t>(item.name.length(), 255));
        u16 reclen = static_cast<u16>((8 + name_len + 1 + 7) & ~7);
        if (reclen < 16) reclen = 16;

        if (bytes_written + reclen > nbytes) {
            break; // Buffer full
        }

        auto* ent = reinterpret_cast<OrbisDirent*>(buf + bytes_written);
        std::memset(ent, 0, reclen);
        ent->d_fileno = static_cast<u32>(1000 + cur_idx);
        ent->d_reclen = reclen;
        ent->d_type = item.is_dir ? 4 : 8; // DT_DIR=4, DT_REG=8
        ent->d_namlen = name_len;
        std::memcpy(ent->d_name, item.name.c_str(), name_len);
        ent->d_name[name_len] = '\0';

        bytes_written += reclen;
        cur_idx++;
    }

    dir.index = cur_idx;
    if (basep) {
        *basep = static_cast<s64>(cur_idx);
    }

    LOG_INFO("KERNEL", "sceKernelGetdirentries: fd=" + std::to_string(fd) + " returned " + 
             std::to_string(bytes_written) + " bytes (advanced to index " + 
             std::to_string(cur_idx) + "/" + std::to_string(dir.entries.size()) + ")");
    return static_cast<s32>(bytes_written);
}

PS4_SYSV_ABI s32 sceKernelGetdents(s32 fd, char* buf, u32 nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

// POSIX Threading
struct HostThreadInfo {
    HANDLE handle{nullptr};
    DWORD id{0};
};

static thread_local HostThreadInfo* t_self_info = nullptr;

struct ThreadWrapperArgs {
    PthreadEntry entry;
    void* arg;
    HostThreadInfo* info;
};

static DWORD WINAPI GuestThreadStub(LPVOID param) {
    auto* args = reinterpret_cast<ThreadWrapperArgs*>(param);
    t_self_info = args->info;
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

    auto* info = new HostThreadInfo();
    auto* wrapper = new ThreadWrapperArgs{ entry, arg, info };

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
        delete info;
        return -1;
    }

    info->handle = hThread;
    info->id = thread_id;

    *thread = info;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadJoin(void* thread, void** value_ptr) {
    if (!thread) return -1;
    auto* info = reinterpret_cast<HostThreadInfo*>(thread);
    if (info->id == GetCurrentThreadId()) return 11; // EDEADLK
    if (info->handle && info->handle != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(info->handle, INFINITE);
        CloseHandle(info->handle);
        info->handle = nullptr;
    }
    return 0;
}

PS4_SYSV_ABI void* scePthreadSelf() {
    if (!t_self_info) {
        t_self_info = new HostThreadInfo();
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                        &t_self_info->handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        t_self_info->id = GetCurrentThreadId();
    }
    return t_self_info;
}

PS4_SYSV_ABI void scePthreadExit(void* value_ptr) {
    ExitThread(0);
}

PS4_SYSV_ABI s32 scePthreadDetach(void* thread) {
    return 0;
}

PS4_SYSV_ABI s32 scePthreadEqual(void* t1, void* t2) {
    if (t1 == t2) return 1;
    if (!t1 || !t2) return 0;
    auto* i1 = reinterpret_cast<HostThreadInfo*>(t1);
    auto* i2 = reinterpret_cast<HostThreadInfo*>(t2);
    return (i1->id == i2->id) ? 1 : 0;
}

PS4_SYSV_ABI void scePthreadYield() {
    SwitchToThread();
}

PS4_SYSV_ABI s32 scePthreadSetaffinity(void* thread, u64 mask) {
    return 0;
}

PS4_SYSV_ABI s32 scePthreadGetaffinity(void* thread, u64* mask) {
    if (mask) *mask = 0xFF;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadSetprio(void* thread, s32 prio) {
    return 0;
}

PS4_SYSV_ABI s32 scePthreadGetprio(void* thread, s32* prio) {
    if (prio) *prio = 0;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadGetthreadid() {
    return static_cast<s32>(GetCurrentThreadId());
}

// Pthread Keys & TLS
PS4_SYSV_ABI s32 scePthreadKeyCreate(u32* key, void (*destructor)(void*)) {
    if (!key) return -1;
    DWORD slot = TlsAlloc();
    if (slot == TLS_OUT_OF_INDEXES) return -1;
    *key = slot;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadKeyDelete(u32 key) {
    return TlsFree(key) ? 0 : -1;
}

PS4_SYSV_ABI void* scePthreadGetspecific(u32 key) {
    return TlsGetValue(key);
}

PS4_SYSV_ABI s32 scePthreadSetspecific(u32 key, const void* value) {
    return TlsSetValue(key, const_cast<void*>(value)) ? 0 : -1;
}

PS4_SYSV_ABI s32 scePthreadOnce(s32* once_control, void (*init_routine)()) {
    if (!once_control || !init_routine) return -1;
    if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(once_control), 1, 0) == 0) {
        init_routine();
        *once_control = 2;
    }
    return 0;
}

// Pthread Attributes
PS4_SYSV_ABI s32 scePthreadAttrInit(void* attr) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrDestroy(void* attr) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrSetstacksize(void* attr, size_t stacksize) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrGetstacksize(const void* attr, size_t* stacksize) {
    if (stacksize) *stacksize = 2 * 1024 * 1024;
    return 0;
}
PS4_SYSV_ABI s32 scePthreadAttrSetdetachstate(void* attr, s32 state) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrSetschedpolicy(void* attr, s32 policy) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrSetschedparam(void* attr, const void* param) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrSetaffinity(void* attr, u64 mask) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrGetaffinity(const void* attr, u64* mask) {
    if (mask) *mask = 0xFF;
    return 0;
}
PS4_SYSV_ABI s32 scePthreadAttrSetinheritsched(void* attr, s32 inherit) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrSetguardsize(void* attr, size_t guardsize) { return 0; }
PS4_SYSV_ABI s32 scePthreadAttrGet(void* thread, void* attr) { return 0; }

// Pthread Mutexes
constexpr u32 ORBIS_MUTEX_MAGIC = 0x4F52424D; // 'ORBM'

struct OrbisMutex {
    u32 magic{ORBIS_MUTEX_MAGIC};
    CRITICAL_SECTION cs;
};

static OrbisMutex* GetOrAllocMutex(void** p_mutex, bool create_if_null = true) {
    if (!p_mutex) return nullptr;
    auto* direct = reinterpret_cast<OrbisMutex*>(p_mutex);
    if (direct->magic == ORBIS_MUTEX_MAGIC) {
        return direct;
    }
    void* ptr = *p_mutex;
    if (ptr) {
        auto* indirect = reinterpret_cast<OrbisMutex*>(ptr);
        if (indirect->magic == ORBIS_MUTEX_MAGIC) {
            return indirect;
        }
    }
    if (create_if_null) {
        auto* m = new OrbisMutex();
        InitializeCriticalSection(&m->cs);
        void* old = InterlockedCompareExchangePointer(p_mutex, m, nullptr);
        if (old != nullptr) {
            DeleteCriticalSection(&m->cs);
            delete m;
            return reinterpret_cast<OrbisMutex*>(old);
        }
        return m;
    }
    return nullptr;
}

PS4_SYSV_ABI s32 scePthreadMutexattrInit(void* attr) { return 0; }
PS4_SYSV_ABI s32 scePthreadMutexattrSettype(void* attr, s32 type) { return 0; }
PS4_SYSV_ABI s32 scePthreadMutexattrSetprotocol(void* attr, s32 protocol) { return 0; }
PS4_SYSV_ABI s32 scePthreadMutexattrDestroy(void* attr) { return 0; }

PS4_SYSV_ABI s32 pthread_mutex_init(void** mutex, const void* attr) {
    if (!mutex) return -1;
    auto* m = new OrbisMutex();
    InitializeCriticalSection(&m->cs);
    *mutex = m;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadMutexInit(void** mutex, const void* attr, const char* name) {
    if (!mutex) return -1;
    auto* m = new OrbisMutex();
    InitializeCriticalSection(&m->cs);
    *mutex = m;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadMutexLock(void** mutex) {
    auto* m = GetOrAllocMutex(mutex, true);
    if (!m) return -1;
    EnterCriticalSection(&m->cs);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadMutexTrylock(void** mutex) {
    auto* m = GetOrAllocMutex(mutex, true);
    if (!m) return -1;
    return TryEnterCriticalSection(&m->cs) ? 0 : -1;
}

PS4_SYSV_ABI s32 scePthreadMutexUnlock(void** mutex) {
    if (!mutex) return 0;
    auto* m = GetOrAllocMutex(mutex, false);
    if (!m) return 0;
    DWORD tid = GetCurrentThreadId();
    if (m->cs.OwningThread != reinterpret_cast<HANDLE>(static_cast<uintptr_t>(tid)) || m->cs.RecursionCount <= 0) {
        return 1; // EPERM
    }
    LeaveCriticalSection(&m->cs);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadMutexDestroy(void** mutex) {
    if (!mutex) return 0;
    auto* m = GetOrAllocMutex(mutex, false);
    if (!m) return 0;
    m->magic = 0;
    DeleteCriticalSection(&m->cs);
    delete m;
    if (*mutex == m) {
        *mutex = nullptr;
    }
    return 0;
}

// Pthread Condition Variables
constexpr u32 ORBIS_COND_MAGIC = 0x4F524356; // 'ORCV'

struct OrbisCond {
    u32 magic{ORBIS_COND_MAGIC};
    CONDITION_VARIABLE cv;
};

static OrbisCond* GetOrAllocCond(void** p_cond, bool create_if_null = true) {
    if (!p_cond) return nullptr;
    auto* direct = reinterpret_cast<OrbisCond*>(p_cond);
    if (direct->magic == ORBIS_COND_MAGIC) return direct;
    void* ptr = *p_cond;
    if (ptr) {
        auto* indirect = reinterpret_cast<OrbisCond*>(ptr);
        if (indirect->magic == ORBIS_COND_MAGIC) return indirect;
    }
    if (create_if_null) {
        auto* c = new OrbisCond();
        InitializeConditionVariable(&c->cv);
        void* old = InterlockedCompareExchangePointer(p_cond, c, nullptr);
        if (old != nullptr) {
            delete c;
            return reinterpret_cast<OrbisCond*>(old);
        }
        return c;
    }
    return nullptr;
}

PS4_SYSV_ABI s32 scePthreadCondattrInit(void* attr) { return 0; }
PS4_SYSV_ABI s32 scePthreadCondattrDestroy(void* attr) { return 0; }

PS4_SYSV_ABI s32 pthread_cond_init(void** cond, const void* attr) {
    if (!cond) return -1;
    auto* c = new OrbisCond();
    InitializeConditionVariable(&c->cv);
    *cond = c;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCondInit(void** cond, const void* attr, const char* name) {
    if (!cond) return -1;
    auto* c = new OrbisCond();
    InitializeConditionVariable(&c->cv);
    *cond = c;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCondDestroy(void** cond) {
    if (!cond) return 0;
    auto* c = GetOrAllocCond(cond, false);
    if (!c) return 0;
    c->magic = 0;
    delete c;
    if (*cond == c) *cond = nullptr;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCondSignal(void** cond) {
    auto* c = GetOrAllocCond(cond, true);
    if (!c) return -1;
    WakeConditionVariable(&c->cv);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCondBroadcast(void** cond) {
    auto* c = GetOrAllocCond(cond, true);
    if (!c) return -1;
    WakeAllConditionVariable(&c->cv);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadCondWait(void** cond, void** mutex) {
    auto* c = GetOrAllocCond(cond, true);
    auto* m = GetOrAllocMutex(mutex, true);
    if (!c || !m) return -1;
    return SleepConditionVariableCS(&c->cv, &m->cs, INFINITE) ? 0 : -1;
}

PS4_SYSV_ABI s32 scePthreadCondTimedwait(void** cond, void** mutex, u64 usec) {
    auto* c = GetOrAllocCond(cond, true);
    auto* m = GetOrAllocMutex(mutex, true);
    if (!c || !m) return -1;
    DWORD ms = static_cast<DWORD>((usec + 999) / 1000);
    return SleepConditionVariableCS(&c->cv, &m->cs, ms) ? 0 : -1;
}

PS4_SYSV_ABI s32 pthread_cond_timedwait(void** cond, void** mutex, const OrbisTimespec* abstime) {
    auto* c = GetOrAllocCond(cond, true);
    auto* m = GetOrAllocMutex(mutex, true);
    if (!c || !m) return -1;
    DWORD ms = 1000;
    if (abstime) {
        OrbisTimespec now;
        sceKernelClockGettime(0, &now);
        s64 diff_sec = abstime->tv_sec - now.tv_sec;
        s64 diff_nsec = abstime->tv_nsec - now.tv_nsec;
        s64 total_ms = (diff_sec * 1000) + (diff_nsec / 1000000);
        ms = (total_ms > 0) ? static_cast<DWORD>(total_ms) : 0;
    }
    return SleepConditionVariableCS(&c->cv, &m->cs, ms) ? 0 : -1;
}

// Pthread Read-Write Locks
constexpr u32 ORBIS_RWLOCK_MAGIC = 0x4F525257; // 'ORRW'

struct OrbisRwlock {
    u32 magic{ORBIS_RWLOCK_MAGIC};
    CRITICAL_SECTION cs;
    std::string name;
};

static OrbisRwlock* GetOrAllocRwlock(void** p_rw, bool create_if_null = true) {
    if (!p_rw) return nullptr;
    auto* direct = reinterpret_cast<OrbisRwlock*>(p_rw);
    if (direct->magic == ORBIS_RWLOCK_MAGIC) return direct;
    void* ptr = *p_rw;
    if (ptr) {
        auto* indirect = reinterpret_cast<OrbisRwlock*>(ptr);
        if (indirect->magic == ORBIS_RWLOCK_MAGIC) return indirect;
    }
    if (create_if_null) {
        auto* rw = new OrbisRwlock();
        InitializeCriticalSection(&rw->cs);
        *p_rw = rw;
        return rw;
    }
    return nullptr;
}

PS4_SYSV_ABI s32 scePthreadRwlockattrInit(void* attr) { return 0; }
PS4_SYSV_ABI s32 scePthreadRwlockattrDestroy(void* attr) { return 0; }

PS4_SYSV_ABI s32 scePthreadRwlockInit(void** rwlock, const void* attr, const char* name) {
    if (!rwlock) return -1;
    auto* rw = new OrbisRwlock();
    InitializeCriticalSection(&rw->cs);
    if (name) rw->name = name;
    *rwlock = rw;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadRwlockDestroy(void** rwlock) {
    if (!rwlock) return 0;
    auto* rw = GetOrAllocRwlock(rwlock, false);
    if (!rw) return 0;
    rw->magic = 0;
    DeleteCriticalSection(&rw->cs);
    delete rw;
    if (*rwlock == rw) *rwlock = nullptr;
    return 0;
}

PS4_SYSV_ABI s32 scePthreadRwlockRdlock(void** rwlock) {
    auto* rw = GetOrAllocRwlock(rwlock, true);
    if (!rw) return -1;
    EnterCriticalSection(&rw->cs);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadRwlockTryrdlock(void** rwlock) {
    auto* rw = GetOrAllocRwlock(rwlock, true);
    if (!rw) return -1;
    return TryEnterCriticalSection(&rw->cs) ? 0 : -1;
}

PS4_SYSV_ABI s32 scePthreadRwlockWrlock(void** rwlock) {
    auto* rw = GetOrAllocRwlock(rwlock, true);
    if (!rw) return -1;
    EnterCriticalSection(&rw->cs);
    return 0;
}

PS4_SYSV_ABI s32 scePthreadRwlockTrywrlock(void** rwlock) {
    auto* rw = GetOrAllocRwlock(rwlock, true);
    if (!rw) return -1;
    return TryEnterCriticalSection(&rw->cs) ? 0 : -1;
}

PS4_SYSV_ABI s32 scePthreadRwlockUnlock(void** rwlock) {
    auto* rw = GetOrAllocRwlock(rwlock, false);
    if (!rw) return 0;
    LeaveCriticalSection(&rw->cs);
    return 0;
}

// Semaphores
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

PS4_SYSV_ABI s32 sceKernelPollSema(void* sema, s32 need) {
    if (!sema) return -1;
    DWORD res = WaitForSingleObject(reinterpret_cast<HANDLE>(sema), 0);
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

// Event Flags
struct OrbisEventFlag {
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
    u64 pattern;
    u32 attr;
    std::string name;
};

PS4_SYSV_ABI s32 sceKernelCreateEventFlag(void** flag, const char* name, u32 attr, u64 init_pattern, const void* opt) {
    if (!flag) return -1;
    auto* ef = new OrbisEventFlag();
    InitializeCriticalSection(&ef->cs);
    InitializeConditionVariable(&ef->cv);
    ef->pattern = init_pattern;
    ef->attr = attr;
    if (name) ef->name = name;
    *flag = ef;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelDeleteEventFlag(void* flag) {
    if (!flag) return 0;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    DeleteCriticalSection(&ef->cs);
    delete ef;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelSetEventFlag(void* flag, u64 pattern) {
    if (!flag) return -1;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    EnterCriticalSection(&ef->cs);
    ef->pattern |= pattern;
    WakeAllConditionVariable(&ef->cv);
    LeaveCriticalSection(&ef->cs);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelClearEventFlag(void* flag, u64 pattern) {
    if (!flag) return -1;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    EnterCriticalSection(&ef->cs);
    ef->pattern &= pattern;
    LeaveCriticalSection(&ef->cs);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelWaitEventFlag(void* flag, u64 bits, u32 wait_mode, u64* out_bits, void* timeout) {
    if (!flag) return -1;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    EnterCriticalSection(&ef->cs);
    bool wait_all = (wait_mode & 0x01);
    bool clear = (wait_mode & 0x10);
    while (true) {
        bool matched = wait_all ? ((ef->pattern & bits) == bits) : ((ef->pattern & bits) != 0);
        if (matched) {
            if (out_bits) *out_bits = ef->pattern;
            if (clear) ef->pattern &= ~bits;
            LeaveCriticalSection(&ef->cs);
            return 0;
        }
        DWORD ms = timeout ? 1000 : INFINITE;
        if (!SleepConditionVariableCS(&ef->cv, &ef->cs, ms)) {
            LeaveCriticalSection(&ef->cs);
            return -1;
        }
    }
}

PS4_SYSV_ABI s32 sceKernelPollEventFlag(void* flag, u64 bits, u32 wait_mode, u64* out_bits) {
    if (!flag) return -1;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    EnterCriticalSection(&ef->cs);
    bool wait_all = (wait_mode & 0x01);
    bool clear = (wait_mode & 0x10);
    bool matched = wait_all ? ((ef->pattern & bits) == bits) : ((ef->pattern & bits) != 0);
    if (matched) {
        if (out_bits) *out_bits = ef->pattern;
        if (clear) ef->pattern &= ~bits;
        LeaveCriticalSection(&ef->cs);
        return 0;
    }
    LeaveCriticalSection(&ef->cs);
    return -1;
}

PS4_SYSV_ABI s32 sceKernelCancelEventFlag(void* flag, u64 pattern, s32* num_threads) {
    if (!flag) return -1;
    auto* ef = static_cast<OrbisEventFlag*>(flag);
    EnterCriticalSection(&ef->cs);
    ef->pattern = pattern;
    WakeAllConditionVariable(&ef->cv);
    LeaveCriticalSection(&ef->cs);
    if (num_threads) *num_threads = 0;
    return 0;
}

// Event Queues
PS4_SYSV_ABI s32 sceKernelCreateEqueue(void** eq, const char* name) {
    if (!eq) return -1;
    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    *eq = reinterpret_cast<void*>(hEvent);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelDeleteEqueue(void* eq) {
    if (eq) CloseHandle(reinterpret_cast<HANDLE>(eq));
    return 0;
}

PS4_SYSV_ABI s32 sceKernelWaitEqueue(void* eq, void* events, s32 num_events, s32* out_events, void* timeout) {
    if (out_events) *out_events = 0;
    if (!eq) return -1;
    DWORD ms = timeout ? 50 : INFINITE;
    DWORD res = WaitForSingleObject(reinterpret_cast<HANDLE>(eq), ms);
    if (res == WAIT_OBJECT_0) {
        if (out_events) *out_events = 1;
        if (events && num_events > 0) {
            struct SceKernelEvent {
                uintptr_t ident;
                short filter;
                unsigned short flags;
                unsigned int fflags;
                intptr_t data;
                void* udata;
            };
            auto* ev = reinterpret_cast<SceKernelEvent*>(events);
            ev[0].ident = reinterpret_cast<uintptr_t>(eq);
            ev[0].filter = -1;
            ev[0].flags = 0;
            ev[0].fflags = 0;
            ev[0].data = 1;
            ev[0].udata = nullptr;
        }
        return 0;
    }
    return (res == WAIT_TIMEOUT) ? 0x8002000B : -1;
}

PS4_SYSV_ABI s32 sceKernelAddUserEvent(void* eq, s32 id) { return 0; }
PS4_SYSV_ABI s32 sceKernelAddReadEvent(void* eq, s32 fd, void* udata) { return 0; }
PS4_SYSV_ABI s32 sceKernelAddWriteEvent(void* eq, s32 fd, void* udata) { return 0; }
PS4_SYSV_ABI s32 sceKernelAddTimerEvent(void* eq, s32 id, u64 time, void* udata) { return 0; }
PS4_SYSV_ABI s32 sceKernelDeleteUserEvent(void* eq, s32 id) { return 0; }
PS4_SYSV_ABI s32 sceKernelDeleteReadEvent(void* eq, s32 fd) { return 0; }
PS4_SYSV_ABI s32 sceKernelDeleteWriteEvent(void* eq, s32 fd) { return 0; }
PS4_SYSV_ABI s32 sceKernelDeleteTimerEvent(void* eq, s32 id) { return 0; }
PS4_SYSV_ABI s32 sceKernelTriggerUserEvent(void* eq, s32 id, void* udata) {
    if (eq) SetEvent(reinterpret_cast<HANDLE>(eq));
    return 0;
}
PS4_SYSV_ABI s32 sceKernelGetEventFilter(void* event) { return 0; }
PS4_SYSV_ABI s32 sceKernelGetEventId(void* event) { return 0; }

// Time & Timing
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

PS4_SYSV_ABI s32 sceKernelClockGettime(s32 clock_id, OrbisTimespec* tp) {
    if (!tp) return -1;
    static LARGE_INTEGER freq = []() {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();

    if (clock_id == 4 || clock_id == 5 || clock_id == 12) {
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        u64 sec = static_cast<u64>(qpc.QuadPart) / freq.QuadPart;
        u64 remainder = static_cast<u64>(qpc.QuadPart) % freq.QuadPart;
        tp->tv_sec = static_cast<s64>(sec);
        tp->tv_nsec = static_cast<s64>((remainder * 1000000000ULL) / freq.QuadPart);
        return 0;
    }

    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    constexpr u64 UNIX_EPOCH_100NS = 116444736000000000ULL;
    u64 t_100ns = (uli.QuadPart >= UNIX_EPOCH_100NS) ? (uli.QuadPart - UNIX_EPOCH_100NS) : 0;
    tp->tv_sec = static_cast<s64>(t_100ns / 10000000ULL);
    tp->tv_nsec = static_cast<s64>((t_100ns % 10000000ULL) * 100);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelGettimeofday(void* tv, void* tz) {
    if (!tv) return -1;
    OrbisTimespec ts;
    sceKernelClockGettime(0, &ts);
    *reinterpret_cast<s64*>(tv) = ts.tv_sec;
    *reinterpret_cast<s64*>(reinterpret_cast<u8*>(tv) + 8) = ts.tv_nsec / 1000;
    return 0;
}

PS4_SYSV_ABI u64 sceKernelGetTscFrequency() {
    return 3000000000ULL; // 3.0 GHz
}

PS4_SYSV_ABI u64 sceKernelReadTsc() {
    return __rdtsc();
}

PS4_SYSV_ABI s32 sceKernelUsleep(u32 useconds) {
    DWORD ms = useconds / 1000;
    Sleep(ms == 0 ? 1 : ms);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelSleep(u32 seconds) {
    Sleep(seconds * 1000);
    return 0;
}

PS4_SYSV_ABI s32 ps4_nanosleep(const OrbisTimespec* req, OrbisTimespec* rem) {
    if (!req) return -1;
    DWORD ms = static_cast<DWORD>((req->tv_sec * 1000) + (req->tv_nsec / 1000000));
    Sleep(ms == 0 ? 1 : ms);
    return 0;
}

// Modules
PS4_SYSV_ABI s32 sceSysmoduleLoadModule(u32 id) {
    return 0; // Success
}

PS4_SYSV_ABI s32 sceSysmoduleUnloadModule(u32 id) {
    return 0;
}

PS4_SYSV_ABI s32 sceSysmoduleIsLoaded(u32 id) {
    return 1;
}

PS4_SYSV_ABI s32 sceKernelLoadStartModule(const char* name, u64 argc, const void* argv, u32 flags, const void* opt, s32* res) {
    if (res) *res = 0;
    return 0x200; // module handle
}

PS4_SYSV_ABI s32 sceKernelStopUnloadModule(s32 id, u64 argc, const void* argv, u32 flags, const void* opt, s32* res) {
    if (res) *res = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelGetModuleInfoFromAddr(const void* addr, u32 flags, void* info) {
    if (info) std::memset(info, 0, 256);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelGetModuleInfoForUnwind(const void* addr, void* info) {
    if (info) std::memset(info, 0, 256);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelInternalMemoryGetModuleSegmentInfo(void* info) {
    if (info) std::memset(info, 0, 256);
    return 0;
}

PS4_SYSV_ABI s32 sceKernelGetProcParam(void* param) {
    if (param) {
        *reinterpret_cast<void**>(param) = reinterpret_cast<void*>(Loader::ElfLoader::Instance().GetProcParamAddress());
    }
    return 0;
}

// CRT & Misc
static thread_local int s_posix_errno = 0;
uintptr_t g_stack_chk_guard = 0x595e9fbd94fda766ULL;
uintptr_t g_stdout_obj = 0;

constexpr u32 ALLOC_MAGIC = 0x5053344D; // "PS4M"
#pragma pack(push, 1)
struct AllocHeader {
    u32 magic;
    u32 padding;
    void* raw_ptr;
    size_t size;
};
#pragma pack(pop)

static void* HostAlloc(size_t size, size_t alignment) {
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    size_t total = size + alignment + sizeof(AllocHeader);
    u8* raw = static_cast<u8*>(std::malloc(total));
    if (!raw) return nullptr;

    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw);
    uintptr_t user_addr = (raw_addr + sizeof(AllocHeader) + (alignment - 1)) & ~(alignment - 1);
    auto* hdr = reinterpret_cast<AllocHeader*>(user_addr - sizeof(AllocHeader));
    hdr->magic = ALLOC_MAGIC;
    hdr->padding = 0;
    hdr->raw_ptr = raw;
    hdr->size = size;

    return reinterpret_cast<void*>(user_addr);
}

static void HostFree(void* ptr) {
    if (!ptr) return;
    auto* hdr = reinterpret_cast<AllocHeader*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AllocHeader));
    if (hdr->magic == ALLOC_MAGIC) {
        std::free(hdr->raw_ptr);
    } else {
        std::free(ptr);
    }
}

PS4_SYSV_ABI void* ps4_malloc(u64 size) {
    return HostAlloc(size, 16);
}

PS4_SYSV_ABI void ps4_free(void* ptr) {
    HostFree(ptr);
}

PS4_SYSV_ABI void* ps4_calloc(u64 num, u64 size) {
    u64 total = num * size;
    void* p = HostAlloc(total, 16);
    if (p) std::memset(p, 0, total);
    return p;
}

PS4_SYSV_ABI void* ps4_realloc(void* ptr, u64 size) {
    if (!ptr) return HostAlloc(size, 16);
    if (size == 0) {
        HostFree(ptr);
        return nullptr;
    }
    auto* hdr = reinterpret_cast<AllocHeader*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AllocHeader));
    void* new_p = HostAlloc(size, 16);
    if (new_p) {
        size_t copy_size = (hdr->magic == ALLOC_MAGIC) ? std::min<size_t>(hdr->size, size) : size;
        std::memcpy(new_p, ptr, copy_size);
        HostFree(ptr);
    }
    return new_p;
}

PS4_SYSV_ABI void* ps4_memalign(u64 alignment, u64 size) {
    return HostAlloc(size, alignment);
}

PS4_SYSV_ABI s32 ps4_posix_memalign(void** memptr, u64 alignment, u64 size) {
    if (!memptr) return 22; // EINVAL
    if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) {
        return 22; // EINVAL
    }
    void* p = HostAlloc(size, alignment);
    if (!p) {
        return 12; // ENOMEM
    }
    *memptr = p;
    return 0; // Success
}

PS4_SYSV_ABI int* __error() {
    return &s_posix_errno;
}

PS4_SYSV_ABI void __stack_chk_fail() {
    LOG_FATAL("CRT", "__stack_chk_fail called by guest! Stack corruption detected.");
    RaiseException(0xC0000409, 0, 0, nullptr);
}

PS4_SYSV_ABI void* __tls_get_addr(void* ti) {
    DWORD slot = Loader::ElfLoader::Instance().GetGuestTlsSlot();
    return TlsGetValue(slot);
}

PS4_SYSV_ABI void ps4_exit(s32 status) {
    ExitProcess(status);
}

PS4_SYSV_ABI s32 sceKernelGetCurrentCpu() {
    return static_cast<s32>(GetCurrentProcessorNumber());
}

PS4_SYSV_ABI s32 sceKernelIsStack(const void* addr) {
    return 1;
}

PS4_SYSV_ABI s32 sceKernelSetGPO(u32 gpo) {
    return 0;
}

PS4_SYSV_ABI s32 sceKernelUuidCreate(void* uuid) {
    if (!uuid) return -1;
    return UuidCreate(reinterpret_cast<UUID*>(uuid)) == RPC_S_OK ? 0 : -1;
}

PS4_SYSV_ABI s32 sceKernelConvertUtcToLocaltime(s64 utc, s64* local) {
    if (local) *local = utc;
    return 0;
}

PS4_SYSV_ABI s32 sceKernelConvertLocaltimeToUtc(s64 local, s64* utc) {
    if (utc) *utc = local;
    return 0;
}

PS4_SYSV_ABI void sceKernelDebugRaiseException(s32 code) {}
PS4_SYSV_ABI void sceKernelDebugRaiseExceptionOnReleaseMode(s32 code) {}
PS4_SYSV_ABI void sceKernelPrintBacktraceWithModuleInfo() {}

// Sockets & Network POSIX
PS4_SYSV_ABI s32 ps4_socket(s32 domain, s32 type, s32 protocol) { return -1; }
PS4_SYSV_ABI s32 ps4_bind(s32 sockfd, const void* addr, u32 addrlen) { return -1; }
PS4_SYSV_ABI s32 ps4_listen(s32 sockfd, s32 backlog) { return -1; }
PS4_SYSV_ABI s32 ps4_accept(s32 sockfd, void* addr, u32* addrlen) { return -1; }
PS4_SYSV_ABI s64 ps4_send(s32 sockfd, const void* buf, u64 len, s32 flags) { return -1; }
PS4_SYSV_ABI s64 ps4_recv(s32 sockfd, void* buf, u64 len, s32 flags) { return -1; }
PS4_SYSV_ABI s32 ps4_shutdown(s32 sockfd, s32 how) { return 0; }
PS4_SYSV_ABI s32 ps4_select(s32 nfds, void* readfds, void* writefds, void* exceptfds, void* timeout) { return 0; }
PS4_SYSV_ABI s32 getrusage(s32 who, void* usage) {
    if (usage) std::memset(usage, 0, 144);
    return 0;
}

// Signals & Exception Handlers
PS4_SYSV_ABI s32 sigprocmask(s32 how, const void* set, void* oset) { return 0; }
PS4_SYSV_ABI s32 _sigprocmask(s32 how, const void* set, void* oset) { return 0; }
PS4_SYSV_ABI s32 sigfillset(void* set) { return 0; }
PS4_SYSV_ABI void* ps4_signal(s32 signum, void* handler) { return nullptr; }
PS4_SYSV_ABI s32 sigreturn(void* ucp) { return 0; }
PS4_SYSV_ABI s32 _is_signal_return() { return 0; }
PS4_SYSV_ABI void __pthread_cxa_finalize(void* dso) {}
PS4_SYSV_ABI s32 __elf_phdr_match_addr(void* addr, void* info) { return 0; }

// System Service, Content, DiscMap & Display
PS4_SYSV_ABI s32 sceDiscMapIsRequestOnHDD(void* request) {
    return 1; // 1 = already on HDD (local disk)
}

PS4_SYSV_ABI s64 sceDiscMapGetPackageSize(void* p) {
    return 27777592; // package size
}

PS4_SYSV_ABI s32 ps4_disc_map_get_location(void* a, u64 b, void* c, u64* out_location, u64* out_lba, u64* out_size) {
    if (out_location) *out_location = 0; // 0 = HDD
    if (out_lba) *out_lba = 0;
    if (out_size) *out_size = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutGetResolutionStatus(s32 handle, void* status) {
    if (status) {
        auto* s = reinterpret_cast<u32*>(status);
        s[0] = 1920; // width
        s[1] = 1080; // height
        s[2] = 1920; // pane width
        s[3] = 1080; // pane height
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSystemServiceParamGetInt(s32 param_id, s32* out_val) {
    if (out_val) {
        *out_val = 1; // English language / valid system setting
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSystemServiceGetStatus(void* status) {
    if (status) {
        std::memset(status, 0, 64); // 0 events pending
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSystemServiceReceiveEvent(void* event) {
    if (event) {
        std::memset(event, 0, 64);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceAppContentAppParamGetInt(s32 param_id, s32* out_val) {
    if (out_val) {
        *out_val = 1;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceAppContentGetAddcontInfoList(s32 a, s32 b, s32 c, u32* out_count) {
    if (out_count) {
        *out_count = 0; // 0 DLC add-on items initially
    }
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceGetEvent(void* event) {
    return static_cast<s32>(0x80960002); // SCE_USER_SERVICE_ERROR_NO_EVENT
}

} // extern "C"

} // namespace Kernel
