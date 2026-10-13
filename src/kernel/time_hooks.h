#pragma once

#include "common/types.h"
#include "video/video_out.h"
#include <windows.h>

namespace Kernel {

class TimeHooks {
public:
    static TimeHooks& Instance();

    bool Install(uintptr_t base_address);
    void SetTargetFps(Video::FrameRateCap cap);
    void SetCutsceneLock(bool locked);

    static PS4_SYSV_ABI float HookGetDeltaTime(float original_dt);
    static PS4_SYSV_ABI u64 HookGetProcessTime();

private:
    TimeHooks() = default;
    ~TimeHooks() = default;

    bool m_installed{false};
    uintptr_t m_base_address{0};
};

} // namespace Kernel
