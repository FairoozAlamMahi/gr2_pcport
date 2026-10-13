#pragma once

#include "common/types.h"
#include <windows.h>

namespace GPU {

void Initialize();
void Shutdown();

// Exported libSceGnmDriver functions
extern "C" {

PS4_SYSV_ABI s32 sceGnmSubmitCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes);
PS4_SYSV_ABI s32 sceGnmSubmitDone();
PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState(void* dcb, u32 num_dwords);
PS4_SYSV_ABI s32 sceGnmInsertPushMarker(void* dcb, const char* name);
PS4_SYSV_ABI s32 sceGnmInsertPopMarker(void* dcb);
PS4_SYSV_ABI s32 sceGnmFlushGarlic();
PS4_SYSV_ABI s32 sceGnmSetCsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetPsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetVsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmDispatchDirect(void* dcb, u32 thread_group_x, u32 thread_group_y, u32 thread_group_z);

} // extern "C"

} // namespace GPU
