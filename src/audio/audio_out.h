#pragma once

#include "common/types.h"
#include <windows.h>

namespace Audio {

struct SceAudioOutOutputParam {
    s32 handle;
    u32 reserved;
    const void* ptr;
};

void Initialize();
void Shutdown();

extern "C" {
PS4_SYSV_ABI s32 sceAudioOutOpen(s32 user_id, s32 type, s32 index, u32 len, u32 freq, u32 param);
PS4_SYSV_ABI s32 sceAudioOutClose(s32 handle);
PS4_SYSV_ABI s32 sceAudioOutOutput(s32 handle, const void* ptr);
PS4_SYSV_ABI s32 sceAudioOutOutputs(SceAudioOutOutputParam* params, u32 num_params);
PS4_SYSV_ABI s32 sceAudioOutGetPortState(s32 handle, void* state);
PS4_SYSV_ABI s32 sceAudio3dPortGetQueueLevel(s32 port, u32* level);
PS4_SYSV_ABI s32 sceNgs2SystemRender(void* sys, void* buf, u32 size);
PS4_SYSV_ABI s32 sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol);
PS4_SYSV_ABI s32 sceAudioOutGetPortTimestamp(s32 handle, u64* timestamp);
}

} // namespace Audio
