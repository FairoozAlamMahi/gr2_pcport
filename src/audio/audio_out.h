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

// libSceAudioOut extensions
PS4_SYSV_ABI s32 sceAudioOutInit();
PS4_SYSV_ABI s32 sceAudioOutSetMixLevelPadSpk(s32 handle, s32 level);

// libSceAudio3d
PS4_SYSV_ABI s32 sceAudio3dInitialize(const void* params);
PS4_SYSV_ABI s32 sceAudio3dGetDefaultOpenParameters(void* params);
PS4_SYSV_ABI s32 sceAudio3dPortOpen(s32 type, const void* params, s32* port_id);
PS4_SYSV_ABI s32 sceAudio3dPortSetAttribute(s32 port, const void* attr);
PS4_SYSV_ABI s32 sceAudio3dObjectReserve(s32 port, s32* obj_id);
PS4_SYSV_ABI s32 sceAudio3dObjectSetAttributes(s32 port, s32 obj_id, const void* attr);
PS4_SYSV_ABI s32 sceAudio3dObjectUnreserve(s32 port, s32 obj_id);
PS4_SYSV_ABI s32 sceAudio3dPortAdvance(s32 port);
PS4_SYSV_ABI s32 sceAudio3dPortClose(s32 port);
PS4_SYSV_ABI s32 sceAudio3dPortPush(s32 port, const void* audio_data, u32 size);
PS4_SYSV_ABI s32 sceAudio3dTerminate();

// libSceNgs2
PS4_SYSV_ABI s32 sceNgs2SystemCreateWithAllocator(const void* desc, const void* alloc, void** handle);
PS4_SYSV_ABI s32 sceNgs2RackCreate(void* sys, const void* desc, void** handle);
PS4_SYSV_ABI s32 sceNgs2RackCreateWithAllocator(void* sys, const void* desc, const void* alloc, void** handle);
PS4_SYSV_ABI s32 sceNgs2RackGetVoiceHandle(void* rack, u32 voice_index, void** voice_handle);
PS4_SYSV_ABI s32 sceNgs2VoiceControl(void* voice, const void* params);
PS4_SYSV_ABI s32 sceNgs2VoiceGetState(void* voice, void* state);
PS4_SYSV_ABI s32 sceNgs2RackQueryBufferSize(const void* desc, usize* buffer_size);
PS4_SYSV_ABI s32 sceNgs2SystemDestroy(void* handle);
PS4_SYSV_ABI s32 sceNgs2RackDestroy(void* rack);
}

} // namespace Audio
