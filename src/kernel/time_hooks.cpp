#include "kernel/time_hooks.h"
#include "kernel/libkernel.h"
#include "common/logger.h"
#include <algorithm>

namespace Kernel {

static inline std::string ToHex(u64 val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
    return std::string(buf);
}

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

    // 60 FPS Engine Mode Patch for Gravity Rush 2 v1.11 (Illusion / EXODUS 60 FPS Mode)
    // 1. Boot game directly into native 60 FPS mode with 60 Hz physics step at 0x454AC5
    u8* fps60_addr = reinterpret_cast<u8*>(base_address + 0x454AC5);
    if (VirtualProtect(fps60_addr, 7, PAGE_EXECUTE_READWRITE, &old_protect)) {
        static const u8 s_fps60_expected[7] = { 0xC6, 0x05, 0xBC, 0x65, 0x6A, 0x01, 0x01 };
        if (std::memcmp(fps60_addr, s_fps60_expected, 7) == 0) {
            static const u8 s_fps60_patch[7] = { 0xC6, 0x05, 0xBC, 0x65, 0x6A, 0x01, 0x00 };
            std::memcpy(fps60_addr, s_fps60_patch, 7);
            FlushInstructionCache(GetCurrentProcess(), fps60_addr, 7);
            LOG_INFO("TIME", "Applied native 60 FPS mode boot patch at 0x454AC5 (movb $0, 0x1AFB088).");
        }
        VirtualProtect(fps60_addr, 7, old_protect, &old_protect);
    }

    // Secondary fallback at 0x854AC5 if mapped with 0x400000 offset
    u8* fps60_alt = reinterpret_cast<u8*>(base_address + 0x854AC5);
    if (VirtualProtect(fps60_alt, 7, PAGE_EXECUTE_READWRITE, &old_protect)) {
        static const u8 s_fps60_alt_exp[7] = { 0x31, 0xF6, 0x45, 0x85, 0xFF, 0x48, 0x8D };
        if (std::memcmp(fps60_alt, s_fps60_alt_exp, 7) == 0) {
            static const u8 s_fps60_alt_pat[7] = { 0xC6, 0x05, 0xBC, 0x65, 0x6A, 0x01, 0x00 };
            std::memcpy(fps60_alt, s_fps60_alt_pat, 7);
            FlushInstructionCache(GetCurrentProcess(), fps60_alt, 7);
            LOG_INFO("TIME", "Applied fallback 60 FPS patch at 0x854AC5.");
        }
        VirtualProtect(fps60_alt, 7, old_protect, &old_protect);
    }

    // 2. Force Flip Rate = 0 (60 Hz) at 0x446980
    // Original: BE 01 00 00 00 (mov $1, %esi -> 30 Hz flip)
    // Patched:  BE 00 00 00 00 (mov $0, %esi -> 60 Hz flip)
    u8* fliprate_addr = reinterpret_cast<u8*>(base_address + 0x446980);
    if (VirtualProtect(fliprate_addr, 5, PAGE_EXECUTE_READWRITE, &old_protect)) {
        static const u8 s_flip_expected[5] = { 0xBE, 0x01, 0x00, 0x00, 0x00 };
        if (std::memcmp(fliprate_addr, s_flip_expected, 5) == 0) {
            static const u8 s_flip_patch[5] = { 0xBE, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(fliprate_addr, s_flip_patch, 5);
            FlushInstructionCache(GetCurrentProcess(), fliprate_addr, 5);
            LOG_INFO("TIME", "Applied 60 Hz fliprate patch at 0x446980 (mov $0, %esi).");
        }
        VirtualProtect(fliprate_addr, 5, old_protect, &old_protect);
    }

    // 3. Render Thread Usleep Throttle Bypass at 0x44DDA4
    // Original: 48 3D 2F 75 00 00 77 0C BF 30 75 00 00 29 C7 E8 48 83 FA 00 (20 bytes: 30ms usleep clamp)
    // Patched:  E9 0F 00 00 00 + 15 NOPs (jmp 0x44DDB8 -> bypasses usleep clamp completely)
    u8* throttle_addr = reinterpret_cast<u8*>(base_address + 0x44DDA4);
    if (VirtualProtect(throttle_addr, 20, PAGE_EXECUTE_READWRITE, &old_protect)) {
        static const u8 s_throttle_expected[6] = { 0x48, 0x3D, 0x2F, 0x75, 0x00, 0x00 };
        if (std::memcmp(throttle_addr, s_throttle_expected, 6) == 0) {
            u8 s_throttle_patch[20];
            s_throttle_patch[0] = 0xE9; // jmp rel32
            s_throttle_patch[1] = 0x0F;
            s_throttle_patch[2] = 0x00;
            s_throttle_patch[3] = 0x00;
            s_throttle_patch[4] = 0x00;
            std::memset(&s_throttle_patch[5], 0x90, 15); // NOP padding
            std::memcpy(throttle_addr, s_throttle_patch, 20);
            FlushInstructionCache(GetCurrentProcess(), throttle_addr, 20);
            LOG_INFO("TIME", "Applied render-thread usleep unthrottle patch at 0x44DDA4 (jmp 0x44DDB8).");
        }
        VirtualProtect(throttle_addr, 20, old_protect, &old_protect);
    }

    // Direct write to internal 60 FPS flag at 0x1AFB088 & 0x18FB088
    for (uintptr_t offset : { (uintptr_t)0x1AFB088, (uintptr_t)0x18FB088 }) {
        u8* flag_addr = reinterpret_cast<u8*>(base_address + offset);
        if (VirtualProtect(flag_addr, 1, PAGE_EXECUTE_READWRITE, &old_protect)) {
            *flag_addr = 0; // 0 = 60 FPS mode
            VirtualProtect(flag_addr, 1, old_protect, &old_protect);
            LOG_INFO("TIME", "Set engine 60 FPS mode flag at 0x" + ToHex(offset) + " = 0.");
        }
    }

    // Half-rate EnemyBrain:getPlayMotionFrame for 60 FPS (0x018C7374 = 30.0f)
    u32* motion_addr = reinterpret_cast<u32*>(base_address + 0x18C7374);
    if (VirtualProtect(motion_addr, 4, PAGE_EXECUTE_READWRITE, &old_protect)) {
        *motion_addr = 0x41F00000; // 30.0f
        VirtualProtect(motion_addr, 4, old_protect, &old_protect);
        LOG_INFO("TIME", "Set motion frame rate at 0x18C7374 = 30.0f (60 FPS motion sync).");
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
