#pragma once

#include "common/types.h"

namespace Video::AvPlayer {

extern "C" {

PS4_SYSV_ABI void* sceAvPlayerInit(const void* initParam);
PS4_SYSV_ABI s32 sceAvPlayerPostInit(void* handle, const void* param);
PS4_SYSV_ABI s32 sceAvPlayerClose(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerAddSource(void* handle, const char* filename);
PS4_SYSV_ABI s32 sceAvPlayerStart(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerStop(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerPause(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerResume(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerIsActive(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerEnableStream(void* handle, u32 stream_id);
PS4_SYSV_ABI s32 sceAvPlayerStreamCount(void* handle);
PS4_SYSV_ABI s32 sceAvPlayerGetStreamInfo(void* handle, u32 stream_id, void* info);
PS4_SYSV_ABI s32 sceAvPlayerGetVideoDataEx(void* handle, void* data);
PS4_SYSV_ABI s32 sceAvPlayerSetLooping(void* handle, bool loop);

} // extern "C"

} // namespace Video::AvPlayer
