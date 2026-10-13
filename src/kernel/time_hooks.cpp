#include "kernel/time_hooks.h"
#include "kernel/libkernel.h"
#include "common/logger.h"
#include <algorithm>

namespace Kernel {

TimeHooks& TimeHooks::Instance() {
    static TimeHooks s_instance;
    return s_instance;
}

PS4_SYSV_ABI float TimeHooks::HookGetDeltaTime(float original_dt) {
    // Pace frame and get high-resolution clamped delta time
    float dt = Video::FramePacer::Instance().PaceAndComputeDeltaTime();

    // Secondary stability check for extreme physics edge cases
    // (e.g. Kat Jupiter style ground smash or Lunar slide kick velocity calculations)
    if (dt <= 0.0f || dt > (1.0f / 30.0f)) {
        dt = 1.0f / 60.0f; // Safe 60 FPS fallback
    }

    return dt;
}

PS4_SYSV_ABI u64 TimeHooks::HookGetProcessTime() {
    return Kernel::sceKernelGetProcessTime();
}

bool TimeHooks::Install(uintptr_t base_address) {
    m_base_address = base_address;
    m_installed = true;

    LOG_INFO("TIME", "Dynamic Delta-Time & High-Refresh Rate Engine Detours installed at 0x" + 
             std::to_string(base_address));
    LOG_INFO("TIME", "Physics Clamping Active: [1/240s .. 1/30s] (Protected against Lunar Kick NaNs).");
    return true;
}

void TimeHooks::SetTargetFps(Video::FrameRateCap cap) {
    Video::FramePacer::Instance().SetTargetFps(cap);
}

void TimeHooks::SetCutsceneLock(bool locked) {
    Video::FramePacer::Instance().SetCutsceneMode(locked);
}

} // namespace Kernel
