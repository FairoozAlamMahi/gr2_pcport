#include "gpu/gnm_driver.h"
#include "video/video_out.h"
#include "memory/vm.h"
#include "common/logger.h"
#include <mutex>
#include <string>
#include <algorithm>
#include <vector>
#include <cstdio>

namespace GPU {

static GpuCapabilities s_caps{};
static PVOID s_veh_handle = nullptr;
static std::mutex s_gpu_mutex;
static u64 s_submitted_dcbs = 0;
static u64 s_submitted_ccbs = 0;
static u64 s_stat_draw_calls = 0;
static u64 s_stat_dispatches = 0;
static u64 s_stat_context_regs = 0;

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

u64 GetSubmittedDcbCount() { return s_submitted_dcbs; }
u64 GetSubmittedCcbCount() { return s_submitted_ccbs; }
u64 GetTotalDrawCalls() { return s_stat_draw_calls; }
u64 GetTotalDispatches() { return s_stat_dispatches; }
u64 GetTotalContextRegChanges() { return s_stat_context_regs; }

static inline std::string ToHex(u64 val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
    return std::string(buf);
}

// PM4 Packet Type-3 Opcodes for AMD Liverpool / Southern Islands / Sea Islands
enum PM4Type3Opcode : u32 {
    IT_NOP = 0x10,
    IT_SET_BASE = 0x11,
    IT_CLEAR_STATE = 0x12,
    IT_INDEX_BUFFER_SIZE = 0x13,
    IT_DISPATCH_DIRECT = 0x15,
    IT_DISPATCH_INDIRECT = 0x16,
    IT_SET_PREDICATION = 0x20,
    IT_DRAW_INDEX_2 = 0x27,
    IT_DRAW_INDEX_INDIRECT = 0x28,
    IT_INDEX_TYPE = 0x2A,
    IT_DRAW_INDEX_AUTO = 0x2D,
    IT_DRAW_INDEX_OFFSET_2 = 0x35,
    IT_WAIT_REG_MEM = 0x3C,
    IT_INDIRECT_BUFFER = 0x3F,
    IT_EVENT_WRITE = 0x46,
    IT_EVENT_WRITE_EOP = 0x47,
    IT_RELEASE_MEM = 0x49,
    IT_ACQUIRE_MEM = 0x58,
    IT_SET_CONFIG_REG = 0x68,
    IT_SET_CONTEXT_REG = 0x69,
    IT_SET_SH_REG = 0x76,
    IT_SET_SH_REG_OFFSET = 0x77
};

static void DecodePM4CommandBuffer(const void* buffer_ptr, u32 size_in_bytes, const char* buffer_type, u32 buffer_index) {
    if (!buffer_ptr || size_in_bytes < 4) return;

    const u8* raw_ptr = static_cast<const u8*>(buffer_ptr);
    uintptr_t uaddr = reinterpret_cast<uintptr_t>(buffer_ptr);

    // If guest virtual address, translate to host pointer
    if (Memory::VirtualMemoryManager::Instance().IsGuestAddress(buffer_ptr)) {
        raw_ptr = static_cast<const u8*>(Memory::VirtualMemoryManager::Instance().GuestToHost(uaddr));
    } else if (uaddr < GUEST_ARENA_BASE) {
        void* translated = Memory::VirtualMemoryManager::Instance().GuestToHost(uaddr);
        if (translated) raw_ptr = static_cast<const u8*>(translated);
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(raw_ptr, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) {
        return;
    }

    const u32* dwords = reinterpret_cast<const u32*>(raw_ptr);
    u32 total_dwords = size_in_bytes / sizeof(u32);
    u32 idx = 0;

    u32 local_draws = 0;
    u32 local_dispatches = 0;
    u32 local_context_regs = 0;

    while (idx < total_dwords) {
        u32 header = dwords[idx];
        u32 type = (header >> 30) & 0x3;

        if (type == 3) {
            u32 count = (header >> 16) & 0x3FFF;
            u32 opcode = (header >> 8) & 0xFF;
            u32 packet_len = count + 2;

            if (idx + packet_len > total_dwords) {
                break;
            }

            const u32* body = &dwords[idx + 1];

            switch (opcode) {
                case IT_SET_CONTEXT_REG: { // 0x69
                    u32 reg_offset = (body[0] & 0xFFFF) + 0xA000;
                    u32 num_regs = count;
                    s_stat_context_regs += num_regs;
                    local_context_regs += num_regs;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_SET_CONTEXT_REG (0x69): Reg 0x" + ToHex(reg_offset) + 
                              ", NumRegs: " + std::to_string(num_regs));
                    break;
                }
                case IT_DRAW_INDEX_AUTO: { // 0x2D
                    u32 index_count = (count >= 1) ? body[0] : 0;
                    u32 draw_initiator = (count >= 2) ? body[1] : 0;
                    s_stat_draw_calls++;
                    local_draws++;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_DRAW_INDEX_AUTO (0x2D): IndexCount: " + std::to_string(index_count) + 
                              ", Initiator: 0x" + ToHex(draw_initiator));
                    break;
                }
                case IT_DISPATCH_DIRECT: { // 0x15
                    u32 gx = (count >= 1) ? body[0] : 0;
                    u32 gy = (count >= 2) ? body[1] : 0;
                    u32 gz = (count >= 3) ? body[2] : 0;
                    s_stat_dispatches++;
                    local_dispatches++;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_DISPATCH_DIRECT (0x15): Groups (" + std::to_string(gx) + 
                              ", " + std::to_string(gy) + ", " + std::to_string(gz) + ")");
                    break;
                }
                case IT_DRAW_INDEX_2: { // 0x27
                    u32 index_count = (count >= 5) ? body[4] : ((count >= 1) ? body[0] : 0);
                    s_stat_draw_calls++;
                    local_draws++;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_DRAW_INDEX_2 (0x27): IndexCount: " + std::to_string(index_count));
                    break;
                }
                case IT_DRAW_INDEX_OFFSET_2: { // 0x35
                    u32 index_count = (count >= 1) ? body[0] : 0;
                    s_stat_draw_calls++;
                    local_draws++;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_DRAW_INDEX_OFFSET_2 (0x35): IndexCount: " + std::to_string(index_count));
                    break;
                }
                case IT_INDEX_TYPE: { // 0x2A
                    u32 index_type = body[0] & 0x3;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_INDEX_TYPE (0x2A): " + (index_type ? "32-bit" : "16-bit"));
                    break;
                }
                case IT_SET_SH_REG: { // 0x76
                    u32 reg_offset = (body[0] & 0xFFFF) + 0x2C00;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_SET_SH_REG (0x76): Reg 0x" + ToHex(reg_offset) + ", NumRegs: " + std::to_string(count));
                    break;
                }
                case IT_SET_CONFIG_REG: { // 0x68
                    u32 reg_offset = (body[0] & 0xFFFF) + 0x2000;
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_SET_CONFIG_REG (0x68): Reg 0x" + ToHex(reg_offset) + ", NumRegs: " + std::to_string(count));
                    break;
                }
                case IT_EVENT_WRITE_EOP: { // 0x47
                    if (count >= 4) {
                        u32 data_sel = (body[2] >> 29) & 0x7;
                        u64 gpu_addr = (static_cast<u64>(body[2] & 0xFFFF) << 32) | (static_cast<u64>(body[1]) & ~0x3ULL);
                        void* host_dest = Memory::VirtualMemoryManager::Instance().GuestToHost(gpu_addr);
                        if (host_dest) {
                            if (data_sel == 1) {
                                *reinterpret_cast<u32*>(host_dest) = body[3];
                            } else {
                                u64 val = (static_cast<u64>(body[4]) << 32) | body[3];
                                *reinterpret_cast<u64*>(host_dest) = val;
                            }
                            LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                                      "] IT_EVENT_WRITE_EOP (0x47): Wrote fence 0x" + 
                                      ToHex((static_cast<u64>(body[4]) << 32) | body[3]) + " to VA 0x" + ToHex(gpu_addr));
                        }
                    }
                    break;
                }
                case IT_RELEASE_MEM: { // 0x49
                    if (count >= 5) {
                        u32 data_sel = (body[1] >> 29) & 0x7;
                        u64 gpu_addr = (static_cast<u64>(body[3] & 0xFFFF) << 32) | (static_cast<u64>(body[2]) & ~0x3ULL);
                        void* host_dest = Memory::VirtualMemoryManager::Instance().GuestToHost(gpu_addr);
                        if (host_dest) {
                            if (data_sel == 1) {
                                *reinterpret_cast<u32*>(host_dest) = body[4];
                            } else {
                                u64 val = (static_cast<u64>(body[5]) << 32) | body[4];
                                *reinterpret_cast<u64*>(host_dest) = val;
                            }
                            LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                                      "] IT_RELEASE_MEM (0x49): Wrote fence 0x" + 
                                      ToHex((static_cast<u64>(body[5]) << 32) | body[4]) + " to VA 0x" + ToHex(gpu_addr));
                        }
                    }
                    break;
                }
                case IT_EVENT_WRITE: { // 0x46
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_EVENT_WRITE (0x46)");
                    break;
                }
                case IT_WAIT_REG_MEM: { // 0x3C
                    LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                              "] IT_WAIT_REG_MEM (0x3C)");
                    break;
                }
                case IT_INDIRECT_BUFFER: { // 0x3F
                    if (count >= 3) {
                        u64 ib_addr = (static_cast<u64>(body[1] & 0xFFFF) << 32) | (static_cast<u64>(body[0]) & ~0x3ULL);
                        u32 ib_size_bytes = (body[2] & 0xFFFFF) * sizeof(u32);
                        void* ib_host = Memory::VirtualMemoryManager::Instance().GuestToHost(ib_addr);
                        if (ib_host && ib_size_bytes > 0) {
                            LOG_DEBUG("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                                      "] Executing Indirect Buffer at VA 0x" + ToHex(ib_addr) + " (" + 
                                      std::to_string(ib_size_bytes) + " bytes)");
                            DecodePM4CommandBuffer(ib_host, ib_size_bytes, "IB", buffer_index);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            idx += packet_len;
        } else if (type == 2) {
            idx += 1;
        } else if (type == 0) {
            u32 count = (header >> 16) & 0x3FFF;
            idx += count + 2;
        } else {
            idx += 1;
        }
    }

    if (local_draws > 0 || local_dispatches > 0) {
        LOG_INFO("PM4", std::string(buffer_type) + "[" + std::to_string(buffer_index) + 
                 "] Processed " + std::to_string(local_draws) + " draw calls, " + 
                 std::to_string(local_dispatches) + " dispatches, " + 
                 std::to_string(local_context_regs) + " context reg updates");
    }
}

static void* s_tessellation_ring_buffer = nullptr;
static u32 s_next_compute_vqid = 1;

extern "C" {

PS4_SYSV_ABI s32 sceGnmSubmitCommandBuffers(u32 count, const void* dcb_gpu_addrs, const u32* dcb_sizes_in_bytes, const void* ccb_gpu_addrs, const u32* ccb_sizes_in_bytes) {
    std::lock_guard<std::mutex> lock(s_gpu_mutex);
    s_submitted_dcbs += count;

    if (dcb_gpu_addrs && dcb_sizes_in_bytes) {
        const uintptr_t* dcb_ptrs = reinterpret_cast<const uintptr_t*>(dcb_gpu_addrs);
        for (u32 i = 0; i < count; ++i) {
            DecodePM4CommandBuffer(reinterpret_cast<const void*>(dcb_ptrs[i]), dcb_sizes_in_bytes[i], "DCB", i);
        }
    }

    if (ccb_gpu_addrs && ccb_sizes_in_bytes) {
        const uintptr_t* ccb_ptrs = reinterpret_cast<const uintptr_t*>(ccb_gpu_addrs);
        s_submitted_ccbs += count;
        for (u32 i = 0; i < count; ++i) {
            DecodePM4CommandBuffer(reinterpret_cast<const void*>(ccb_ptrs[i]), ccb_sizes_in_bytes[i], "CCB", i);
        }
    }

    return 0;
}

PS4_SYSV_ABI s32 sceGnmSubmitDone() {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState(void* dcb, u32 num_dwords) {
    if (dcb && num_dwords > 0) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        static constexpr u32 s_clear_seq[] = {
            0xc0012800u, 0x80000000u, 0x80000000u, 0xc0001200u, 0u, 0xc0055800u,
            0x2ec47fc0u, 0xffffffffu, 0u,          0u,          0u, 10u
        };
        u32 copy_count = std::min(num_dwords, static_cast<u32>(sizeof(s_clear_seq) / sizeof(u32)));
        std::memcpy(cmdbuf, s_clear_seq, copy_count * sizeof(u32));
    }
    return static_cast<s32>(num_dwords);
}

PS4_SYSV_ABI s32 sceGnmDrawInitDefaultHardwareState350(void* dcb, u32 num_dwords) {
    return sceGnmDrawInitDefaultHardwareState(dcb, num_dwords);
}

PS4_SYSV_ABI s32 sceGnmDispatchInitDefaultHardwareState(void* dcb, u32 num_dwords) {
    if (dcb && num_dwords >= 8) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (1U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[1] = 0x216u; // COMPUTE_STATIC_THREAD_MGMT_SE0
        cmdbuf[2] = 0xffffffffu;
        cmdbuf[3] = (3U << 30) | (1U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[4] = 0x217u; // COMPUTE_STATIC_THREAD_MGMT_SE1
        cmdbuf[5] = 0xffffffffu;
        cmdbuf[6] = (3U << 30) | (0U << 16) | (IT_NOP << 8);
        cmdbuf[7] = 0;
    }
    return static_cast<s32>(num_dwords);
}

PS4_SYSV_ABI s32 sceGnmInsertPushMarker(void* dcb, const char* name) {
    (void)dcb;
    (void)name;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertPopMarker(void* dcb) {
    (void)dcb;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmFlushGarlic() {
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetCsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetCsShaderWithModifier(void* dcb, u32 num_dwords, const void* cs_regs, u32 modifier) {
    if (dcb && num_dwords >= 8 && cs_regs) {
        const u32* regs = static_cast<const u32*>(cs_regs);
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[1] = 0x20cu; // COMPUTE_PGM_LO
        cmdbuf[2] = regs[0];
        cmdbuf[3] = 0;
        u32 rsrc1 = (modifier == 0) ? regs[2] : ((regs[2] & 0xfffffc3fu) | modifier);
        cmdbuf[4] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[5] = 0x212u; // COMPUTE_PGM_RSRC1 / RSRC2
        cmdbuf[6] = rsrc1;
        cmdbuf[7] = regs[3];
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetPsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetPsShader350(void* dcb, u32 num_dwords, const void* ps_regs) {
    if (dcb && num_dwords >= 8 && ps_regs) {
        const u32* regs = static_cast<const u32*>(ps_regs);
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[1] = 8u; // SPI_SHADER_PGM_LO_PS
        cmdbuf[2] = regs[0];
        cmdbuf[3] = 0;
        cmdbuf[4] = (3U << 30) | (1U << 16) | (IT_SET_CONTEXT_REG << 8);
        cmdbuf[5] = 0x203u - 0xA000; // DB_SHADER_CONTROL
        cmdbuf[6] = (num_dwords >= 11) ? regs[10] : 0;
        cmdbuf[7] = (3U << 30) | (0U << 16) | (IT_NOP << 8);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmUpdatePsShader350(void* dcb, u32 num_dwords, const void* ps_regs) {
    return sceGnmSetPsShader350(dcb, num_dwords, ps_regs);
}

PS4_SYSV_ABI s32 sceGnmSetVsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetEmbeddedVsShader(void* dcb, u32 num_dwords, u32 shader_id, u32 shader_modifier) {
    LOG_DEBUG("GPU", "sceGnmSetEmbeddedVsShader called (shader_id=" + std::to_string(shader_id) + 
                     ", modifier=" + std::to_string(shader_modifier) + ")");
    if (dcb && num_dwords >= 8) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[1] = 0x08u; // SPI_SHADER_PGM_LO_VS
        cmdbuf[2] = 0xfe000f10u; // Embedded fullscreen quad shader base
        cmdbuf[3] = 0;
        cmdbuf[4] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[5] = 0x0au; // SPI_SHADER_PGM_RSRC1_VS
        cmdbuf[6] = 0x000c0000u;
        cmdbuf[7] = 4u;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndexAuto(void* dcb, u32 num_dwords, u32 index_count, u32 flags) {
    if (dcb && num_dwords >= 7) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (1U << 16) | (IT_DRAW_INDEX_AUTO << 8) | 2U;
        cmdbuf[1] = index_count;
        cmdbuf[2] = flags | 2U; // Draw initiator
        for (u32 i = 3; i < num_dwords && i < 7; ++i) {
            cmdbuf[i] = (3U << 30) | (0U << 16) | (IT_NOP << 8);
        }
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDispatchDirect(void* dcb, u32 thread_group_x, u32 thread_group_y, u32 thread_group_z) {
    (void)dcb;
    (void)thread_group_x;
    (void)thread_group_y;
    (void)thread_group_z;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSubmitAndFlipCommandBuffers(
    u32 count,
    const void* dcb_gpu_addrs,
    const u32* dcb_sizes_in_bytes,
    const void* ccb_gpu_addrs,
    const u32* ccb_sizes_in_bytes,
    u32 video_handle,
    u32 display_buffer_index,
    u32 flip_mode,
    s64 flip_arg
) {
    if (s_submitted_dcbs <= 20 || s_submitted_dcbs % 60 == 0) {
        LOG_INFO("GPU", "sceGnmSubmitAndFlipCommandBuffers: buffer_index=" + std::to_string(display_buffer_index) +
                        ", count=" + std::to_string(count) + ", flip_mode=" + std::to_string(flip_mode));
    }
    sceGnmSubmitCommandBuffers(count, dcb_gpu_addrs, dcb_sizes_in_bytes, ccb_gpu_addrs, ccb_sizes_in_bytes);
    Video::sceVideoOutSubmitFlip(static_cast<s32>(video_handle), static_cast<s32>(display_buffer_index), flip_mode, flip_arg);
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDingDong(u32 ring_id, u32 dword_offset) {
    (void)ring_id;
    (void)dword_offset;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertWaitFlipDone(void* dcb, s32 video_handle, s32 display_buffer_index) {
    (void)dcb;
    (void)video_handle;
    (void)display_buffer_index;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmMapComputeQueue(u32 pipe_id, u32 queue_id, const void* ring_base_addr, u32 ring_size_dw, void* read_ptr_addr) {
    std::lock_guard<std::mutex> lock(s_gpu_mutex);
    u32 vqid = s_next_compute_vqid++;
    if (read_ptr_addr) {
        *reinterpret_cast<u32*>(read_ptr_addr) = 0;
    }
    LOG_INFO("GPU", "sceGnmMapComputeQueue: ASC pipe " + std::to_string(pipe_id) + 
                    " queue " + std::to_string(queue_id) + " mapped to vqueue " + std::to_string(vqid) +
                    " (RingSizeDW=" + std::to_string(ring_size_dw) + ")");
    return static_cast<s32>(vqid);
}

PS4_SYSV_ABI s32 sceGnmMapComputeQueueWithPriority(u32 pipe_id, u32 queue_id, const void* ring_base_addr, u32 ring_size_dw, void* read_ptr_addr, u32 priority) {
    (void)priority;
    return sceGnmMapComputeQueue(pipe_id, queue_id, ring_base_addr, ring_size_dw, read_ptr_addr);
}

PS4_SYSV_ABI void* sceGnmGetTheTessellationFactorRingBufferBaseAddress() {
    if (!s_tessellation_ring_buffer) {
        s_tessellation_ring_buffer = Memory::VirtualMemoryManager::Instance().AllocateFlexibleMemory(0x800000, 0);
        if (!s_tessellation_ring_buffer) {
            alignas(256) static u8 s_fallback_tess_ring[0x100000];
            s_tessellation_ring_buffer = s_fallback_tess_ring;
        }
        LOG_INFO("GPU", "Tessellation Factor Ring Buffer initialized at: 0x" + 
                 ToHex(reinterpret_cast<uintptr_t>(s_tessellation_ring_buffer)));
    }
    return s_tessellation_ring_buffer;
}

PS4_SYSV_ABI s32 sceGnmAreSubmitsAllowed() {
    return 1;
}

PS4_SYSV_ABI s32 sceGnmBeginWorkload(void* arg0, void* arg1, void* arg2) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmEndWorkload(void* arg0, void* arg1, void* arg2) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmAddEqEvent(void* arg0, void* arg1) {
    (void)arg0;
    (void)arg1;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDeleteEqEvent(void* arg0) {
    (void)arg0;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmGetGpuBlockStatus(void* arg0, void* arg1) {
    (void)arg0;
    (void)arg1;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndex(void* dcb, u32 num_dwords, u32 index_count, const void* index_addr, u32 flags) {
    if (dcb && num_dwords >= 4) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        uintptr_t addr = reinterpret_cast<uintptr_t>(index_addr);
        cmdbuf[0] = (3U << 30) | (3U << 16) | (IT_DRAW_INDEX_2 << 8); // 0x27
        cmdbuf[1] = static_cast<u32>(addr);
        cmdbuf[2] = static_cast<u32>(addr >> 32);
        cmdbuf[3] = index_count;
        if (num_dwords > 4) cmdbuf[4] = flags;
        for (u32 i = 5; i < num_dwords; ++i) {
            cmdbuf[i] = (3U << 30) | (0U << 16) | (IT_NOP << 8);
        }
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndexOffset(void* dcb, u32 num_dwords, u32 index_count, u32 offset, u32 flags) {
    if (dcb && num_dwords >= 3) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (2U << 16) | (IT_DRAW_INDEX_OFFSET_2 << 8); // 0x35
        cmdbuf[1] = index_count;
        cmdbuf[2] = offset;
        if (num_dwords > 3) cmdbuf[3] = flags;
        for (u32 i = 4; i < num_dwords; ++i) {
            cmdbuf[i] = (3U << 30) | (0U << 16) | (IT_NOP << 8);
        }
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndirect(void* dcb, u32 num_dwords, const void* arg2, const void* arg3) {
    (void)dcb;
    (void)num_dwords;
    (void)arg2;
    (void)arg3;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndexIndirect(void* dcb, u32 num_dwords, const void* arg2, const void* arg3) {
    (void)dcb;
    (void)num_dwords;
    (void)arg2;
    (void)arg3;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndexMultiInstanced(void* dcb, u32 num_dwords, u32 index_count, u32 instance_count, u32 first_index, u32 vertex_offset, u32 first_instance) {
    (void)dcb;
    (void)num_dwords;
    (void)index_count;
    (void)instance_count;
    (void)first_index;
    (void)vertex_offset;
    (void)first_instance;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndexIndirectMulti(void* dcb, u32 num_dwords, const void* arg2, u32 count, u32 stride) {
    (void)dcb;
    (void)num_dwords;
    (void)arg2;
    (void)count;
    (void)stride;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawIndirectMulti(void* dcb, u32 num_dwords, const void* arg2, u32 count, u32 stride) {
    (void)dcb;
    (void)num_dwords;
    (void)arg2;
    (void)count;
    (void)stride;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawInitToDefaultContextState(void* dcb, u32 num_dwords) {
    return sceGnmDrawInitDefaultHardwareState(dcb, num_dwords);
}

PS4_SYSV_ABI s32 sceGnmSetEmbeddedPsShader(void* dcb, u32 num_dwords, u32 shader_id, u32 shader_modifier) {
    (void)shader_id;
    (void)shader_modifier;
    if (dcb && num_dwords >= 8) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[1] = 0x00u; // SPI_SHADER_PGM_LO_PS
        cmdbuf[2] = 0xfe000000u;
        cmdbuf[3] = 0;
        cmdbuf[4] = (3U << 30) | (2U << 16) | (IT_SET_SH_REG << 8);
        cmdbuf[5] = 0x02u; // SPI_SHADER_PGM_RSRC1_PS
        cmdbuf[6] = 0x000c0000u;
        cmdbuf[7] = 4u;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmUpdateVsShader(void* dcb, const void* vs_regs, u32 shader_modifier) {
    (void)dcb;
    (void)vs_regs;
    (void)shader_modifier;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetEsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetGsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetHsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetLsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDrawOpaqueAuto(void* dcb, u32 num_dwords) {
    if (dcb && num_dwords >= 2) {
        u32* cmdbuf = static_cast<u32*>(dcb);
        cmdbuf[0] = (3U << 30) | (0U << 16) | (IT_DRAW_INDEX_AUTO << 8);
        cmdbuf[1] = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDispatchIndirect(void* dcb, u32 num_dwords, u64 data_offset) {
    (void)dcb;
    (void)num_dwords;
    (void)data_offset;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDispatchIndirectOnMec(void* dcb, u32 num_dwords, u64 data_offset) {
    (void)dcb;
    (void)num_dwords;
    (void)data_offset;
    return 0;
}

PS4_SYSV_ABI void* sceGnmGetShaderProgramBaseAddress(u32 type) {
    (void)type;
    static u8 s_dummy_shader_arena[65536]{};
    return s_dummy_shader_arena;
}

PS4_SYSV_ABI s32 sceGnmGetGpuInfoStatus(void* status, u32 size) {
    if (status && size > 0) std::memset(status, 0, size);
    return 0;
}

PS4_SYSV_ABI s32 sceGnmGetShaderStatus(void* status) {
    if (status) *reinterpret_cast<u32*>(status) = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertPushColorMarker(void* dcb, const char* name, u32 color) {
    (void)dcb;
    (void)name;
    (void)color;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertSetColorMarker(void* dcb, const char* name, u32 color) {
    (void)dcb;
    (void)name;
    (void)color;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertSetMarker(void* dcb, const char* name) {
    (void)dcb;
    (void)name;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmInsertThreadTraceMarker(void* dcb, u32 marker) {
    (void)dcb;
    (void)marker;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetVgtControl(void* dcb, u32 vgt_control) {
    (void)dcb;
    (void)vgt_control;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmResetVgtControl(void* dcb) {
    (void)dcb;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetWaveLimitMultipliers(void* dcb, u32 multipliers) {
    (void)dcb;
    (void)multipliers;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmUnmapComputeQueue(u32 vqid) {
    (void)vqid;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmRequestFlipAndSubmitDone(u32 video_handle, u32 display_buffer_index, u32 flip_mode, s64 flip_arg) {
    return Video::sceVideoOutSubmitFlip(static_cast<s32>(video_handle), static_cast<s32>(display_buffer_index), flip_mode, flip_arg);
}

PS4_SYSV_ABI s32 sceGnmCreateWorkloadStream(void* stream, u32 size) {
    (void)stream;
    (void)size;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDestroyWorkloadStream(void* stream) {
    (void)stream;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmGetNumTcaUnits() {
    return 2;
}

PS4_SYSV_ABI s32 sceGnmIsUserPaEnabled() {
    return 0;
}

PS4_SYSV_ABI u32 sceGnmLogicalCuIndexToPhysicalCuIndex(u32 cu) {
    return cu;
}

PS4_SYSV_ABI u32 sceGnmLogicalCuMaskToPhysicalCuMask(u32 mask) {
    return mask;
}

PS4_SYSV_ABI s32 sceGnmUpdateGsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmUpdateHsShader(void* dcb, const void* shader) {
    (void)dcb;
    (void)shader;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetGsRingSizes(void* dcb, u32 ring_sizes) {
    (void)dcb;
    (void)ring_sizes;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDebugHardwareStatus(void* status) {
    if (status) *reinterpret_cast<u32*>(status) = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmDisableMipStatsReport(void* dcb) {
    (void)dcb;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmSetupMipStatsReport(void* dcb, const void* report) {
    (void)dcb;
    (void)report;
    return 0;
}

PS4_SYSV_ABI s32 sceGnmRequestMipStatsReportAndReset(void* dcb, const void* report) {
    (void)dcb;
    (void)report;
    return 0;
}

// Diagnostics & Razor Profiling Stubs
PS4_SYSV_ABI s32 sceRazorIsLoaded() { return 0; }
PS4_SYSV_ABI s32 sceRazorCaptureImmediate(const char* label) { (void)label; return 0; }
PS4_SYSV_ABI s32 sceGnmDriverCaptureInProgress() { return 0; }
PS4_SYSV_ABI s32 sceGnmDriverTraceInProgress() { return 0; }
PS4_SYSV_ABI s32 sceGnmDriverTriggerCapture() { return 0; }
PS4_SYSV_ABI u32 sceGnmGetEqEventType(const void* eq_event) { (void)eq_event; return 0; }
PS4_SYSV_ABI u64 sceGnmGetEqTimeStamp(const void* eq_event) { (void)eq_event; return 0ULL; }
PS4_SYSV_ABI u64 sceGnmGetLastWaitedAddress() { return 0ULL; }
PS4_SYSV_ABI u64 sceGnmGetProtectionFaultTimeStamp() { return 0ULL; }

} // extern "C"

} // namespace GPU
