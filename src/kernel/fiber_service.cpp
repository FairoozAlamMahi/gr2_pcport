#include "kernel/fiber_service.h"
#include "common/logger.h"
#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <string>

namespace Fiber {

struct HostFiber {
    void* win_fiber{nullptr};
    void* entry{nullptr};
    u64 init_arg{0};
    u64 run_arg{0};
    u64 ret_arg{0};
    void* caller_fiber{nullptr};
    std::string name;
    bool completed{false};
};

static std::mutex s_fiber_mutex;
static std::unordered_map<void*, HostFiber*> s_fibers;
static thread_local void* t_thread_as_fiber = nullptr;
static thread_local HostFiber* t_current_host_fiber = nullptr;

static void EnsureThreadIsFiber() {
    if (!t_thread_as_fiber) {
        t_thread_as_fiber = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (!t_thread_as_fiber) {
            t_thread_as_fiber = GetCurrentFiber();
        }
    }
}

static VOID CALLBACK WinFiberProc(PVOID lpParameter) {
    auto* hf = static_cast<HostFiber*>(lpParameter);
    t_current_host_fiber = hf;
    using PS4FiberEntry = void (PS4_SYSV_ABI *)(u64 init_arg, u64 run_arg);
    auto entry_fn = reinterpret_cast<PS4FiberEntry>(hf->entry);
    if (entry_fn) {
        entry_fn(hf->init_arg, hf->run_arg);
    }
    hf->completed = true;
    t_current_host_fiber = nullptr;
    if (hf->caller_fiber) {
        SwitchToFiber(hf->caller_fiber);
    }
}

extern "C" {

PS4_SYSV_ABI s32 _sceFiberInitializeImpl(void* fiber, const char* name, void* entry, u64 argOnInitialize, void* addrContext, u64 sizeContext, const void* params) {
    (void)addrContext;
    (void)sizeContext;
    (void)params;
    if (!fiber || !entry) return -1;

    EnsureThreadIsFiber();

    auto* hf = new HostFiber();
    hf->entry = entry;
    hf->init_arg = argOnInitialize;
    hf->name = name ? name : "anonymous_fiber";
    hf->win_fiber = CreateFiberEx(64 * 1024, 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, WinFiberProc, hf);

    {
        std::lock_guard<std::mutex> lock(s_fiber_mutex);
        s_fibers[fiber] = hf;
    }

    LOG_INFO("FIBER", "_sceFiberInitializeImpl: name=" + hf->name);
    return 0; // SCE_OK
}

PS4_SYSV_ABI s32 sceFiberRun(void* fiber, u64 argOnRun, u64* argOnReturn) {
    if (!fiber) return -1;
    EnsureThreadIsFiber();

    HostFiber* hf = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_fiber_mutex);
        auto it = s_fibers.find(fiber);
        if (it != s_fibers.end()) {
            hf = it->second;
        }
    }
    if (!hf || !hf->win_fiber) return -1;

    hf->caller_fiber = GetCurrentFiber();
    hf->run_arg = argOnRun;
    t_current_host_fiber = hf;

    SwitchToFiber(hf->win_fiber);

    if (argOnReturn) {
        *argOnReturn = hf->ret_arg;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFiberSwitch(void* fiber, u64 argOnRun, u64* argOnReturn) {
    return sceFiberRun(fiber, argOnRun, argOnReturn);
}

PS4_SYSV_ABI s32 sceFiberReturnToThread(u64 argOnReturn, const void* params) {
    (void)params;
    HostFiber* hf = t_current_host_fiber;
    if (!hf || !hf->caller_fiber) return -1;

    hf->ret_arg = argOnReturn;
    t_current_host_fiber = nullptr;
    SwitchToFiber(hf->caller_fiber);
    return 0;
}

PS4_SYSV_ABI s32 sceFiberFinalize(void* fiber) {
    if (!fiber) return -1;
    HostFiber* hf = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_fiber_mutex);
        auto it = s_fibers.find(fiber);
        if (it != s_fibers.end()) {
            hf = it->second;
            s_fibers.erase(it);
        }
    }
    if (hf) {
        if (hf->win_fiber) {
            DeleteFiber(hf->win_fiber);
        }
        delete hf;
    }
    return 0;
}

} // extern "C"

} // namespace Fiber
