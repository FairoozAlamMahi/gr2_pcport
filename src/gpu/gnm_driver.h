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

u64 GetSubmittedDcbCount();
u64 GetSubmittedCcbCount();
u64 GetTotalDrawCalls();
u64 GetTotalDispatches();
u64 GetTotalContextRegChanges();

// Exported libSceGnmDriver functions
extern "C" {

PS4_SYSV_ABI s32 sceGnmSubmitCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes);
PS4_SYSV_ABI s32 sceGnmSubmitDone();
PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState(void* dcb, u32 num_dwords);
PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState350(void* dcb, u32 num_dwords);
PS4_SYSV_ABI s32 sceGnmDispatchInitDefaultHardwareState(void* dcb, u32 num_dwords);
PS4_SYSV_ABI s32 sceGnmInsertPushMarker(void* dcb, const char* name);
PS4_SYSV_ABI s32 sceGnmInsertPopMarker(void* dcb);
PS4_SYSV_ABI s32 sceGnmFlushGarlic();
PS4_SYSV_ABI s32 sceGnmSetCsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetCsShaderWithModifier(void* dcb, u32 num_dwords, const void* cs_regs, u32 modifier);
PS4_SYSV_ABI s32 sceGnmSetPsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetPsShader350(void* dcb, u32 num_dwords, const void* ps_regs);
PS4_SYSV_ABI s32 sceGnmUpdatePsShader350(void* dcb, u32 num_dwords, const void* ps_regs);
PS4_SYSV_ABI s32 sceGnmSetVsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetEmbeddedVsShader(void* dcb, u32 num_dwords, u32 shader_id, u32 shader_modifier);
PS4_SYSV_ABI s32 sceGnmDrawIndexAuto(void* dcb, u32 num_dwords, u32 index_count, u32 flags);
PS4_SYSV_ABI s32 sceGnmDispatchDirect(void* dcb, u32 thread_group_x, u32 thread_group_y, u32 thread_group_z);
PS4_SYSV_ABI s32 sceGnmSubmitAndFlipCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes, u32 video_handle, u32 display_buffer_index, u32 flip_mode, s64 flip_arg);
PS4_SYSV_ABI s32 sceGnmDingDong(u32 ring_id, u32 dword_offset);
PS4_SYSV_ABI s32 sceGnmInsertWaitFlipDone(void* dcb, s32 video_handle, s32 display_buffer_index);
PS4_SYSV_ABI s32 sceGnmMapComputeQueue(u32 pipe_id, u32 queue_id, const void* ring_base_addr, u32 ring_size_dw, void* read_ptr_addr);
PS4_SYSV_ABI s32 sceGnmMapComputeQueueWithPriority(u32 pipe_id, u32 queue_id, const void* ring_base_addr, u32 ring_size_dw, void* read_ptr_addr, u32 priority);
PS4_SYSV_ABI void* sceGnmGetTheTessellationFactorRingBufferBaseAddress();

PS4_SYSV_ABI s32 sceGnmAreSubmitsAllowed();
PS4_SYSV_ABI s32 sceGnmBeginWorkload(void* arg0, void* arg1, void* arg2);
PS4_SYSV_ABI s32 sceGnmEndWorkload(void* arg0, void* arg1, void* arg2);
PS4_SYSV_ABI s32 sceGnmAddEqEvent(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceGnmDeleteEqEvent(void* arg0);
PS4_SYSV_ABI s32 sceGnmGetGpuBlockStatus(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceGnmDrawIndex(void* dcb, u32 num_dwords, u32 index_count, const void* index_addr, u32 flags);
PS4_SYSV_ABI s32 sceGnmDrawIndexOffset(void* dcb, u32 num_dwords, u32 index_count, u32 offset, u32 flags);
PS4_SYSV_ABI s32 sceGnmDrawIndirect(void* dcb, u32 num_dwords, const void* arg2, const void* arg3);
PS4_SYSV_ABI s32 sceGnmDrawIndexIndirect(void* dcb, u32 num_dwords, const void* arg2, const void* arg3);
PS4_SYSV_ABI s32 sceGnmDrawIndexMultiInstanced(void* dcb, u32 num_dwords, u32 index_count, u32 instance_count, u32 first_index, u32 vertex_offset, u32 first_instance);
PS4_SYSV_ABI s32 sceGnmDrawIndexIndirectMulti(void* dcb, u32 num_dwords, const void* arg2, u32 count, u32 stride);
PS4_SYSV_ABI s32 sceGnmDrawIndirectMulti(void* dcb, u32 num_dwords, const void* arg2, u32 count, u32 stride);
PS4_SYSV_ABI s32 sceGnmDrawInitToDefaultContextState(void* dcb, u32 num_dwords);

PS4_SYSV_ABI s32 sceGnmSetEmbeddedPsShader(void* dcb, u32 num_dwords, u32 shader_id, u32 shader_modifier);
PS4_SYSV_ABI s32 sceGnmUpdateVsShader(void* dcb, const void* vs_regs, u32 shader_modifier);
PS4_SYSV_ABI s32 sceGnmSetEsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetGsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetHsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetLsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmDrawOpaqueAuto(void* dcb, u32 num_dwords);
PS4_SYSV_ABI s32 sceGnmDispatchIndirect(void* dcb, u32 num_dwords, u64 data_offset);
PS4_SYSV_ABI s32 sceGnmDispatchIndirectOnMec(void* dcb, u32 num_dwords, u64 data_offset);
PS4_SYSV_ABI void* sceGnmGetShaderProgramBaseAddress(u32 type);
PS4_SYSV_ABI s32 sceGnmGetGpuInfoStatus(void* status, u32 size);
PS4_SYSV_ABI s32 sceGnmGetShaderStatus(void* status);
PS4_SYSV_ABI s32 sceGnmInsertPushColorMarker(void* dcb, const char* name, u32 color);
PS4_SYSV_ABI s32 sceGnmInsertSetColorMarker(void* dcb, const char* name, u32 color);
PS4_SYSV_ABI s32 sceGnmInsertSetMarker(void* dcb, const char* name);
PS4_SYSV_ABI s32 sceGnmInsertThreadTraceMarker(void* dcb, u32 marker);
PS4_SYSV_ABI s32 sceGnmSetVgtControl(void* dcb, u32 vgt_control);
PS4_SYSV_ABI s32 sceGnmResetVgtControl(void* dcb);
PS4_SYSV_ABI s32 sceGnmSetWaveLimitMultipliers(void* dcb, u32 multipliers);
PS4_SYSV_ABI s32 sceGnmUnmapComputeQueue(u32 vqid);
PS4_SYSV_ABI s32 sceGnmRequestFlipAndSubmitDone(u32 video_handle, u32 display_buffer_index, u32 flip_mode, s64 flip_arg);
PS4_SYSV_ABI s32 sceGnmCreateWorkloadStream(void* stream, u32 size);
PS4_SYSV_ABI s32 sceGnmDestroyWorkloadStream(void* stream);
PS4_SYSV_ABI s32 sceGnmGetNumTcaUnits();
PS4_SYSV_ABI s32 sceGnmIsUserPaEnabled();
PS4_SYSV_ABI u32 sceGnmLogicalCuIndexToPhysicalCuIndex(u32 cu);
PS4_SYSV_ABI u32 sceGnmLogicalCuMaskToPhysicalCuMask(u32 mask);
PS4_SYSV_ABI s32 sceGnmUpdateGsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmUpdateHsShader(void* dcb, const void* shader);
PS4_SYSV_ABI s32 sceGnmSetGsRingSizes(void* dcb, u32 ring_sizes);
PS4_SYSV_ABI s32 sceGnmDebugHardwareStatus(void* status);
PS4_SYSV_ABI s32 sceGnmDisableMipStatsReport(void* dcb);
PS4_SYSV_ABI s32 sceGnmSetupMipStatsReport(void* dcb, const void* report);
PS4_SYSV_ABI s32 sceGnmRequestMipStatsReportAndReset(void* dcb, const void* report);

// Diagnostics & Razor Profiling Stubs
PS4_SYSV_ABI s32 sceRazorIsLoaded();
PS4_SYSV_ABI s32 sceRazorCaptureImmediate(const char* label);
PS4_SYSV_ABI s32 sceGnmDriverCaptureInProgress();
PS4_SYSV_ABI s32 sceGnmDriverTraceInProgress();
PS4_SYSV_ABI s32 sceGnmDriverTriggerCapture();
PS4_SYSV_ABI u32 sceGnmGetEqEventType(const void* eq_event);
PS4_SYSV_ABI u64 sceGnmGetEqTimeStamp(const void* eq_event);
PS4_SYSV_ABI u64 sceGnmGetLastWaitedAddress();
PS4_SYSV_ABI u64 sceGnmGetProtectionFaultTimeStamp();

} // extern "C"

} // namespace GPU
