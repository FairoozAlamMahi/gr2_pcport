#pragma once

#include "common/types.h"
#include <windows.h>
#include <string>

namespace GPU {

enum class GpuVendor {
    Unknown,
    NVIDIA,
    AMD,
    Intel,
    Generic
};

enum class MemoryModel {
    UniversalCoherentStaging, // Safe across all GPUs & integrated graphics
    MirroredVehStaging,       // NVIDIA optimization (bypasses sparse binding driver stalls)
    DirectDeviceVisible       // AMD / ReBAR native memory mapping (matches PS4 APU)
};

struct GpuCapabilities {
    GpuVendor vendor{GpuVendor::Unknown};
    std::string device_name{"Universal Vulkan Display Device"};
    u32 vendor_id{0};
    u32 device_id{0};
    u32 subgroup_size{32};
    bool is_discrete{true};
    bool native_wave64{false};
    MemoryModel memory_model{MemoryModel::UniversalCoherentStaging};
};

void Initialize(const std::string& vendor_override = "auto");
void Shutdown();

const GpuCapabilities& GetCapabilities();

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
PS4_SYSV_ABI s32 sceGnmSubmitAndFlipCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes, u32 video_handle, u32 display_buffer_index, u32 flip_mode, s64 flip_arg);
PS4_SYSV_ABI s32 sceGnmDingDong(u32 ring_id, u32 dword_offset);
PS4_SYSV_ABI s32 sceGnmInsertWaitFlipDone(void* dcb, s32 video_handle, s32 display_buffer_index);

} // extern "C"

} // namespace GPU
