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

    // Patch triple-buffering GPU fence spin loop at 0x44E0A0
    // Original: 48 8B 3E 48 39 D7 75 F8 (mov (%rsi), %rdi; cmp %rdx, %rdi; jne 44e0a0)
    // Patched:  48 89 16 48 89 D7 90 90 (mov %rdx, (%rsi); mov %rdx, %rdi; nop; nop)
    u8* target_addr = reinterpret_cast<u8*>(base_address + 0x44E0A0);
    DWORD old_protect;
    if (VirtualProtect(target_addr, 8, PAGE_EXECUTE_READWRITE, &old_protect)) {
        static const u8 s_expected[8] = { 0x48, 0x8B, 0x3E, 0x48, 0x39, 0xD7, 0x75, 0xF8 };
        if (std::memcmp(target_addr, s_expected, 8) == 0) {
            static const u8 s_patch[8] = { 0x48, 0x89, 0x16, 0x48, 0x89, 0xD7, 0x90, 0x90 };
            std::memcpy(target_addr, s_patch, 8);
            FlushInstructionCache(GetCurrentProcess(), target_addr, 8);
            LOG_INFO("TIME", "Applied auto-satisfaction patch to GPU fence spin loop at 0x44E0A0.");
        }
        VirtualProtect(target_addr, 8, old_protect, &old_protect);
    }

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
