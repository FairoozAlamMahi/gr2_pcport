#pragma once

#include "common/types.h"
#include <windows.h>

namespace Audio {

void Initialize();
void Shutdown();

extern "C" {
PS4_SYSV_ABI s32 sceAudioOutOpen(s32 user_id, s32 type, s32 index, u32 len, u32 freq, u32 param);
PS4_SYSV_ABI s32 sceAudioOutClose(s32 handle);
PS4_SYSV_ABI s32 sceAudioOutOutput(s32 handle, const void* ptr);
PS4_SYSV_ABI s32 sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol);
PS4_SYSV_ABI s32 sceAudioOutGetPortTimestamp(s32 handle, u64* timestamp);
}

} // namespace Audio
