#pragma once

#include "common/types.h"
#include <windows.h>

namespace Video {

enum class FrameRateCap {
    Cap30 = 30,
    Cap60 = 60,
    Cap120 = 120,
    Cap144 = 144,
    Uncapped = 0
};

class FramePacer {
public:
    static FramePacer& Instance();

    void Initialize(FrameRateCap cap = FrameRateCap::Cap60);
    void SetTargetFps(FrameRateCap cap);
    void SetCutsceneMode(bool in_cutscene);

    // Call on flip to pace frame and compute clamped delta-time
    float PaceAndComputeDeltaTime();

private:
    FramePacer() = default;
    ~FramePacer() = default;

    FrameRateCap m_target_cap{FrameRateCap::Cap60};
    bool m_in_cutscene{false};
    HANDLE m_waitable_timer{NULL};
    LARGE_INTEGER m_qpc_freq{};
    LARGE_INTEGER m_last_qpc{};
};

void Initialize();
void Shutdown();

struct SceVideoOutBufferAttribute {
    u32 pixelFormat;
    u32 tilingMode;
    u32 aspectRatio;
    u32 width;
    u32 height;
    u32 pitchInPixel;
    u32 reserved[2];
};

extern "C" {
PS4_SYSV_ABI s32 sceVideoOutOpen(s32 user_id, s32 bus_type, s32 index, const void* param);
PS4_SYSV_ABI s32 sceVideoOutClose(s32 handle);
PS4_SYSV_ABI s32 sceVideoOutSetBufferAttribute(SceVideoOutBufferAttribute* attribute, u32 pixelFormat, u32 tilingMode, u32 aspectRatio, u32 width, u32 height, u32 pitchInPixel);
PS4_SYSV_ABI s32 sceVideoOutRegisterBuffers(s32 handle, s32 startIndex, void* const* addresses, s32 bufferNum, const SceVideoOutBufferAttribute* attribute);
PS4_SYSV_ABI s32 sceVideoOutAddFlipEvent(void* eq, s32 handle, void* udata);
PS4_SYSV_ABI s32 sceVideoOutSubmitFlip(s32 handle, s32 buffer_index, u32 flip_mode, s64 flip_arg);
PS4_SYSV_ABI s32 sceVideoOutSetFlipRate(s32 handle, s32 flip_rate);
PS4_SYSV_ABI s32 sceVideoOutGetVblankStatus(s32 handle, void* status);
}

} // namespace Video
