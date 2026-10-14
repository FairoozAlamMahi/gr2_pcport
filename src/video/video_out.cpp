#include "video/video_out.h"
#include "video/vulkan_context.h"
#include "common/logger.h"
#include <algorithm>

namespace Video {

FramePacer& FramePacer::Instance() {
    static FramePacer s_instance;
    return s_instance;
}

void FramePacer::Initialize(FrameRateCap cap) {
    m_target_cap = cap;
    QueryPerformanceFrequency(&m_qpc_freq);
    QueryPerformanceCounter(&m_last_qpc);

    // Create high-resolution waitable timer if available
    m_waitable_timer = CreateWaitableTimerExW(
        nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );

    if (!m_waitable_timer) {
        m_waitable_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }

    LOG_INFO("VIDEO", "Frame Pacer initialized with target FPS cap: " + 
             (cap == FrameRateCap::Uncapped ? "Uncapped" : std::to_string(static_cast<int>(cap))));
}

void FramePacer::SetTargetFps(FrameRateCap cap) {
    m_target_cap = cap;
    LOG_INFO("VIDEO", "Target FPS cap changed to: " + 
             (cap == FrameRateCap::Uncapped ? "Uncapped" : std::to_string(static_cast<int>(cap))));
}

void FramePacer::SetCutsceneMode(bool in_cutscene) {
    m_in_cutscene = in_cutscene;
}

float FramePacer::PaceAndComputeDeltaTime() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    double elapsed_sec = static_cast<double>(now.QuadPart - m_last_qpc.QuadPart) / 
                         static_cast<double>(m_qpc_freq.QuadPart);

    // Determine effective target frame time
    double target_frame_time = 0.0;
    if (m_in_cutscene) {
        // Cutscenes and comic panels strictly clamped to 60 FPS (16.66 ms)
        target_frame_time = 1.0 / 60.0;
    } else if (m_target_cap != FrameRateCap::Uncapped) {
        target_frame_time = 1.0 / static_cast<double>(m_target_cap);
    }

    // High-resolution pacing wait
    if (target_frame_time > 0.0 && elapsed_sec < target_frame_time) {
        double wait_sec = target_frame_time - elapsed_sec;
        if (m_waitable_timer && wait_sec > 0.001) {
            LARGE_INTEGER due_time;
            due_time.QuadPart = -static_cast<LONGLONG>((wait_sec - 0.0005) * 10000000.0);
            SetWaitableTimer(m_waitable_timer, &due_time, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(m_waitable_timer, INFINITE);
        }

        // Spin-wait remainder for microsecond precision
        do {
            QueryPerformanceCounter(&now);
            elapsed_sec = static_cast<double>(now.QuadPart - m_last_qpc.QuadPart) / 
                          static_cast<double>(m_qpc_freq.QuadPart);
        } while (elapsed_sec < target_frame_time);
    }

    m_last_qpc = now;

    // Strict physics stability clamping: [1/240.0f, 1/30.0f]
    // Prevents Lunar Slide Kick division-by-zero (NaN) crashes at high FPS
    float raw_dt = static_cast<float>(elapsed_sec);
    float clamped_dt = std::clamp(raw_dt, 1.0f / 240.0f, 1.0f / 30.0f);

    return clamped_dt;
}

void Initialize() {
    FramePacer::Instance().Initialize(FrameRateCap::Cap60);
    // Initialize Vulkan presentation context (1080p 60 FPS standard)
    VulkanContext::Instance().Initialize(1920, 1080, false, false);
    LOG_INFO("VIDEO", "libSceVideoOut subsystem initialized.");
}

void Shutdown() {
    VulkanContext::Instance().Shutdown();
    LOG_INFO("VIDEO", "libSceVideoOut subsystem shut down.");
}

static std::vector<void*> s_registered_buffers;
static SceVideoOutBufferAttribute s_buffer_attr{};
static void* s_flip_equeue = nullptr;
static void* s_flip_udata = nullptr;
static u64 s_flip_count = 0;

static inline std::string ToHex(u64 val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
    return std::string(buf);
}

extern "C" {

PS4_SYSV_ABI s32 sceVideoOutOpen(s32 user_id, s32 bus_type, s32 index, const void* param) {
    LOG_INFO("VIDEO", "sceVideoOutOpen called (user_id=" + std::to_string(user_id) + ")");
    return 1; // Video handle 1
}

PS4_SYSV_ABI s32 sceVideoOutClose(s32 handle) {
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutSetBufferAttribute(
    SceVideoOutBufferAttribute* attribute,
    u32 pixelFormat,
    u32 tilingMode,
    u32 aspectRatio,
    u32 width,
    u32 height,
    u32 pitchInPixel
) {
    if (!attribute) return -1;
    attribute->pixelFormat = pixelFormat;
    attribute->tilingMode = tilingMode;
    attribute->aspectRatio = aspectRatio;
    attribute->width = width;
    attribute->height = height;
    attribute->pitchInPixel = pitchInPixel;
    s_buffer_attr = *attribute;
    LOG_INFO("VIDEO", "sceVideoOutSetBufferAttribute: " + std::to_string(width) + "x" + 
             std::to_string(height) + ", pitch=" + std::to_string(pitchInPixel) + 
             ", format=0x" + ToHex(pixelFormat) + ", tiling=" + std::to_string(tilingMode));
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutRegisterBuffers(
    s32 handle,
    s32 startIndex,
    void* const* addresses,
    s32 bufferNum,
    const SceVideoOutBufferAttribute* attribute
) {
    if (attribute) s_buffer_attr = *attribute;
    if (addresses && bufferNum > 0) {
        if (s_registered_buffers.size() < static_cast<size_t>(startIndex + bufferNum)) {
            s_registered_buffers.resize(startIndex + bufferNum, nullptr);
        }
        for (s32 i = 0; i < bufferNum; ++i) {
            s_registered_buffers[startIndex + i] = addresses[i];
            LOG_INFO("VIDEO", "Registered display buffer " + std::to_string(startIndex + i) + 
                     " at VA 0x" + ToHex(reinterpret_cast<uintptr_t>(addresses[i])));
        }
    }
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutAddFlipEvent(void* eq, s32 handle, void* udata) {
    s_flip_equeue = eq;
    s_flip_udata = udata;
    (void)s_flip_udata;
    LOG_INFO("VIDEO", "sceVideoOutAddFlipEvent registered with equeue 0x" + 
             ToHex(reinterpret_cast<uintptr_t>(eq)));
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutSubmitFlip(s32 handle, s32 buffer_index, u32 flip_mode, s64 flip_arg) {
    s_flip_count++;
    void* frame_data = nullptr;
    if (buffer_index >= 0 && static_cast<size_t>(buffer_index) < s_registered_buffers.size()) {
        frame_data = s_registered_buffers[buffer_index];
    }

    if (s_flip_count <= 20 || s_flip_count % 60 == 0) {
        LOG_INFO("VIDEO", "sceVideoOutSubmitFlip: Presented display buffer " + std::to_string(buffer_index) + 
                          " (Frame #" + std::to_string(s_flip_count) + ")");
    }

    if (VulkanContext::Instance().IsInitialized()) {
        VulkanContext::Instance().ProcessWindowMessages();
        VulkanContext::Instance().PresentFrame(frame_data);
    }

    // Signal flip event if registered
    if (s_flip_equeue) {
        SetEvent(reinterpret_cast<HANDLE>(s_flip_equeue));
    }

    // Pace frame and compute delta-time
    float dt = FramePacer::Instance().PaceAndComputeDeltaTime();
    (void)dt;
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutSetFlipRate(s32 handle, s32 flip_rate) {
    // flip_rate 0 = 60 Hz, flip_rate 1 = 30 Hz (often set during comic book panels or cutscenes)
    if (flip_rate > 0) {
        FramePacer::Instance().SetCutsceneMode(true);
    } else {
        FramePacer::Instance().SetCutsceneMode(false);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceVideoOutGetVblankStatus(s32 handle, void* status) {
    return 0;
}

} // extern "C"

} // namespace Video
