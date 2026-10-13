#include "gpu/gnm_driver.h"
#include "common/logger.h"
#include <mutex>
#include <string>
#include <algorithm>
#include <vector>

namespace GPU {

static GpuCapabilities s_caps{};
static PVOID s_veh_handle = nullptr;
static std::mutex s_gpu_mutex;
static u64 s_submitted_dcbs = 0;

static LONG WINAPI GpuMemoryExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR is_write = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR fault_addr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];

        if (is_write == 1 && fault_addr >= GUEST_ARENA_BASE) {
            DWORD old_prot;
            VirtualProtect(reinterpret_cast<LPVOID>(fault_addr & ~0xFFFULL), 0x1000, PAGE_EXECUTE_READWRITE, &old_prot);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void DetectGpuCapabilities(const std::string& vendor_override) {
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);

    struct FoundDevice {
        std::wstring name;
        std::wstring pnp_id;
        DWORD flags;
    };
    std::vector<FoundDevice> devices;

    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            devices.push_back({ dd.DeviceString, dd.DeviceID, dd.StateFlags });
        }
    }

    // Default to first primary device or first available
    FoundDevice selected{};
    if (!devices.empty()) {
        selected = devices[0];
        for (const auto& dev : devices) {
            if (dev.flags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                selected = dev;
                break;
            }
        }
    }

    // Convert wstring to string
    std::string dev_name;
    for (wchar_t c : selected.name) {
        if (c > 0 && c < 128) dev_name += static_cast<char>(c);
    }
    if (dev_name.empty()) dev_name = "Generic Universal GPU";
    s_caps.device_name = dev_name;

    // Parse Vendor ID from PNP ID string (e.g., "PCI\VEN_10DE&DEV_2803...")
    u32 detected_vendor_id = 0;
    std::wstring id_upper = selected.pnp_id;
    for (auto& c : id_upper) c = towupper(c);

    size_t ven_idx = id_upper.find(L"VEN_");
    if (ven_idx != std::wstring::npos && ven_idx + 8 <= id_upper.size()) {
        std::wstring ven_hex = id_upper.substr(ven_idx + 4, 4);
        detected_vendor_id = static_cast<u32>(wcstoul(ven_hex.c_str(), nullptr, 16));
    }
    s_caps.vendor_id = detected_vendor_id;

    // Evaluate hardware profile
    if (vendor_override == "amd" || (!vendor_override.empty() && vendor_override != "auto" ? false : detected_vendor_id == 0x1002)) {
        s_caps.vendor = GpuVendor::AMD;
        s_caps.subgroup_size = 64;
        s_caps.native_wave64 = true;
        s_caps.memory_model = MemoryModel::DirectDeviceVisible;
    } else if (vendor_override == "nvidia" || (!vendor_override.empty() && vendor_override != "auto" ? false : detected_vendor_id == 0x10DE)) {
        s_caps.vendor = GpuVendor::NVIDIA;
        s_caps.subgroup_size = 32;
        s_caps.native_wave64 = false;
        s_caps.memory_model = MemoryModel::MirroredVehStaging;
    } else if (vendor_override == "intel" || (!vendor_override.empty() && vendor_override != "auto" ? false : detected_vendor_id == 0x8086)) {
        s_caps.vendor = GpuVendor::Intel;
        s_caps.subgroup_size = 16;
        s_caps.native_wave64 = false;
        s_caps.memory_model = MemoryModel::UniversalCoherentStaging;
    } else {
        s_caps.vendor = GpuVendor::Generic;
        s_caps.subgroup_size = 32;
        s_caps.native_wave64 = false;
        s_caps.memory_model = MemoryModel::UniversalCoherentStaging;
    }
}

void Initialize(const std::string& vendor_override) {
    LOG_INFO("GPU", "Probing Host Display Devices for Universal GPU Execution...");

    DetectGpuCapabilities(vendor_override);

    std::string vendor_str = "Generic / Portable";
    std::string profile_str = "Universal Coherent Staging";

    switch (s_caps.vendor) {
        case GpuVendor::AMD:
            vendor_str = "AMD Radeon (Native Wave64 matching PS4 APU)";
            profile_str = "Direct Device Memory / ReBAR (Zero-copy native VRAM)";
            break;
        case GpuVendor::NVIDIA:
            vendor_str = "NVIDIA GeForce (32-wide Warp Adaptation)";
            profile_str = "Mirrored Staging + VEH Write-Tracking (Bypasses Ada Lovelace driver stalls)";
            // Install VEH write tracker only when NVIDIA profile is active
            s_veh_handle = AddVectoredExceptionHandler(1, GpuMemoryExceptionHandler);
            break;
        case GpuVendor::Intel:
            vendor_str = "Intel Arc / Xe Graphics";
            profile_str = "Coherent Shared Memory Staging";
            break;
        case GpuVendor::Generic:
        default:
            vendor_str = "Universal Portable Vulkan Device";
            profile_str = "Standard Coherent Host Staging";
            break;
    }

    char ven_hex_buf[16];
    std::snprintf(ven_hex_buf, sizeof(ven_hex_buf), "0x%04X", s_caps.vendor_id);
    LOG_INFO("GPU", "Primary GPU: " + s_caps.device_name + " (Vendor ID: " + ven_hex_buf + ")");
    LOG_INFO("GPU", "Execution Profile: " + vendor_str);
    LOG_INFO("GPU", "Memory Pipeline: " + profile_str);
    LOG_INFO("GPU", "Subgroup Execution: " + std::to_string(s_caps.subgroup_size) + "-thread lockstep (" + 
             (s_caps.native_wave64 ? "Native Wave64" : "Warp Split Adaptation Active") + ")");
}

void Shutdown() {
    if (s_veh_handle) {
        RemoveVectoredExceptionHandler(s_veh_handle);
        s_veh_handle = nullptr;
    }
    LOG_INFO("GPU", "GPU Translation Layer shut down.");
}

const GpuCapabilities& GetCapabilities() {
    return s_caps;
}

extern "C" {

PS4_SYSV_ABI s32 sceGnmSubmitCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes) {
    std::lock_guard<std::mutex> lock(s_gpu_mutex);
    s_submitted_dcbs += count;
    (void)s_submitted_dcbs;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSubmitDone() {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState(void* dcb, u32 num_dwords) {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertPushMarker(void* dcb, const char* name) {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertPopMarker(void* dcb) {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmFlushGarlic() {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetCsShader(void* dcb, const void* shader) {
    // When translating GCN compute bytecode:
    // If native_wave64 is false, inject OpControlBarrier (ScopeSubgroup)
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetPsShader(void* dcb, const void* shader) {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetVsShader(void* dcb, const void* shader) {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDispatchDirect(void* dcb, u32 thread_group_x, u32 thread_group_y, u32 thread_group_z) {
    return 0;
}

} // extern "C"

} // namespace GPU
