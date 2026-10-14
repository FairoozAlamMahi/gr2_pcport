#include "audio/audio_out.h"
#include "common/logger.h"
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace Audio {

struct AudioPort {
    s32 handle{0};
    u32 sample_len{512};
    u32 sample_rate{48000};
    u64 last_hash{0};
    std::chrono::steady_clock::time_point last_submit{};
};

static std::mutex s_audio_mutex;
static std::unordered_map<s32, AudioPort> s_ports;
static s32 s_next_handle = 1;

void Initialize() {
    LOG_INFO("AUDIO", "libSceAudioOut subsystem initialized with Gravity Kick deduplication filter.");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(s_audio_mutex);
    s_ports.clear();
    LOG_INFO("AUDIO", "libSceAudioOut subsystem shut down.");
}

static u64 ComputeAudioHash(const u8* data, usize len) {
    u64 hash = 14695981039346656037ULL;
    usize step = (len > 256) ? (len / 64) : 4;
    for (usize i = 0; i < len; i += step) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

extern "C" {

PS4_SYSV_ABI s32 sceAudioOutOpen(s32 user_id, s32 type, s32 index, u32 len, u32 freq, u32 param) {
    std::lock_guard<std::mutex> lock(s_audio_mutex);
    s32 handle = s_next_handle++;

    AudioPort port{};
    port.handle = handle;
    port.sample_len = len > 0 ? len : 512;
    port.sample_rate = freq > 0 ? freq : 48000;
    port.last_submit = std::chrono::steady_clock::now();

    s_ports[handle] = port;
    LOG_INFO("AUDIO", "Opened audio port " + std::to_string(handle) + 
             " (rate=" + std::to_string(port.sample_rate) + "Hz, len=" + std::to_string(port.sample_len) + ")");
    return handle;
}

PS4_SYSV_ABI s32 sceAudioOutClose(s32 handle) {
    std::lock_guard<std::mutex> lock(s_audio_mutex);
    s_ports.erase(handle);
    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutOutput(s32 handle, const void* ptr) {
    if (!ptr) return -1;

    std::lock_guard<std::mutex> lock(s_audio_mutex);
    auto it = s_ports.find(handle);
    if (it == s_ports.end()) return -1;

    auto& port = it->second;
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - port.last_submit).count();

    // Gravity Kick Audio Overlap Filter:
    // If consecutive audio packets have identical sound hashes submitted within 80 ms,
    // suppress buffer stacking to prevent audio loop saturation on PC audio drivers.
    usize byte_len = port.sample_len * 2 * sizeof(s16); // Stereo 16-bit
    u64 current_hash = ComputeAudioHash(reinterpret_cast<const u8*>(ptr), byte_len);

    if (current_hash == port.last_hash && elapsed_ms < 80) {
        // Suppress redundant overlapping sound cue
        return 0;
    }

    port.last_hash = current_hash;
    port.last_submit = now;

    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutOutputs(SceAudioOutOutputParam* params, u32 num_params) {
    if (!params || num_params == 0) return 0;
    for (u32 i = 0; i < num_params; ++i) {
        sceAudioOutOutput(params[i].handle, params[i].ptr);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutGetPortState(s32 handle, void* state) {
    if (state) {
        auto* s = reinterpret_cast<u32*>(state);
        s[0] = 0; // ready / idle
        s[1] = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceAudio3dPortGetQueueLevel(s32 port, u32* level) {
    if (level) *level = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2SystemRender(void* sys, void* buf, u32 size) {
    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol) {
    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutGetPortTimestamp(s32 handle, u64* timestamp) {
    if (!timestamp) return -1;
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    *timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
    return 0;
}

} // extern "C"

} // namespace Audio
