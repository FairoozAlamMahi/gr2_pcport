#include "audio/audio_out.h"
#include "common/logger.h"
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <mmdeviceapi.h>
#include <audioclient.h>

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

// WASAPI Host Playback State
static bool s_wasapi_initialized = false;
static bool s_com_initialized = false;
static IMMDeviceEnumerator* s_enumerator = nullptr;
static IMMDevice* s_device = nullptr;
static IAudioClient* s_audio_client = nullptr;
static IAudioRenderClient* s_render_client = nullptr;
static UINT32 s_buffer_frame_count = 0;

static bool InitWasapi() {
    if (s_wasapi_initialized) return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        s_com_initialized = true;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&s_enumerator));
    if (FAILED(hr) || !s_enumerator) {
        LOG_WARN("AUDIO", "Failed to create IMMDeviceEnumerator: hr=0x" + std::to_string(hr));
        return false;
    }

    hr = s_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &s_device);
    if (FAILED(hr) || !s_device) {
        LOG_WARN("AUDIO", "Failed to get default audio endpoint: hr=0x" + std::to_string(hr));
        return false;
    }

    hr = s_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&s_audio_client));
    if (FAILED(hr) || !s_audio_client) {
        LOG_WARN("AUDIO", "Failed to activate IAudioClient: hr=0x" + std::to_string(hr));
        return false;
    }

    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 48000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8); // 4 bytes
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; // 192000
    wfx.cbSize = 0;

    REFERENCE_TIME buffer_duration = 1000000; // 100 ms in 100ns units

    // Try AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM (0x80000000) | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY (0x08000000)
    DWORD stream_flags = 0x80000000 | 0x08000000;
    hr = s_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, buffer_duration, 0, &wfx, nullptr);
    if (FAILED(hr)) {
        // Fallback without autoconvert
        hr = s_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, &wfx, nullptr);
    }

    if (FAILED(hr)) {
        LOG_WARN("AUDIO", "IAudioClient::Initialize failed: hr=0x" + std::to_string(hr));
        return false;
    }

    hr = s_audio_client->GetBufferSize(&s_buffer_frame_count);
    if (FAILED(hr)) {
        LOG_WARN("AUDIO", "Failed to get audio buffer size: hr=0x" + std::to_string(hr));
        return false;
    }

    hr = s_audio_client->GetService(__uuidof(IAudioRenderClient),
                                    reinterpret_cast<void**>(&s_render_client));
    if (FAILED(hr) || !s_render_client) {
        LOG_WARN("AUDIO", "Failed to get IAudioRenderClient: hr=0x" + std::to_string(hr));
        return false;
    }

    hr = s_audio_client->Start();
    if (FAILED(hr)) {
        LOG_WARN("AUDIO", "IAudioClient::Start failed: hr=0x" + std::to_string(hr));
        return false;
    }

    s_wasapi_initialized = true;
    LOG_INFO("AUDIO", "WASAPI host audio playback started (48000 Hz, 2 ch, 16-bit PCM, bufferFrames=" +
             std::to_string(s_buffer_frame_count) + ").");
    return true;
}

static void ShutdownWasapi() {
    if (!s_wasapi_initialized) return;

    if (s_audio_client) {
        s_audio_client->Stop();
    }
    if (s_render_client) {
        s_render_client->Release();
        s_render_client = nullptr;
    }
    if (s_audio_client) {
        s_audio_client->Release();
        s_audio_client = nullptr;
    }
    if (s_device) {
        s_device->Release();
        s_device = nullptr;
    }
    if (s_enumerator) {
        s_enumerator->Release();
        s_enumerator = nullptr;
    }
    if (s_com_initialized) {
        CoUninitialize();
        s_com_initialized = false;
    }
    s_wasapi_initialized = false;
    s_buffer_frame_count = 0;
    LOG_INFO("AUDIO", "WASAPI host audio playback shut down.");
}

void Initialize() {
    InitWasapi();
    LOG_INFO("AUDIO", "libSceAudioOut subsystem initialized with Gravity Kick deduplication filter and WASAPI playback.");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(s_audio_mutex);
    ShutdownWasapi();
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

    // Route audio to host playback device via WASAPI when received from port 1
    if (handle == 1 && s_wasapi_initialized && s_render_client && s_audio_client) {
        UINT32 padding = 0;
        HRESULT hr = s_audio_client->GetCurrentPadding(&padding);
        if (SUCCEEDED(hr)) {
            UINT32 available = (s_buffer_frame_count > padding) ? (s_buffer_frame_count - padding) : 0;
            if (available < port.sample_len) {
                // Buffer is temporarily full; wait briefly for host audio engine to consume frames
                for (int retry = 0; retry < 5 && available < port.sample_len; ++retry) {
                    Sleep(1);
                    if (SUCCEEDED(s_audio_client->GetCurrentPadding(&padding))) {
                        available = (s_buffer_frame_count > padding) ? (s_buffer_frame_count - padding) : 0;
                    }
                }
            }

            if (available >= port.sample_len) {
                BYTE* pData = nullptr;
                hr = s_render_client->GetBuffer(port.sample_len, &pData);
                if (SUCCEEDED(hr) && pData) {
                    std::memcpy(pData, ptr, byte_len);
                    s_render_client->ReleaseBuffer(port.sample_len, 0);
                }
            }
        }
    }

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

// libSceAudioOut extensions
PS4_SYSV_ABI s32 sceAudioOutInit() {
    Initialize();
    return 0;
}

PS4_SYSV_ABI s32 sceAudioOutSetMixLevelPadSpk(s32 handle, s32 level) {
    (void)handle;
    (void)level;
    return 0;
}

// libSceAudio3d
static std::atomic<s32> s_next_audio3d_port{1};

PS4_SYSV_ABI s32 sceAudio3dInitialize(const void* params) {
    (void)params;
    LOG_INFO("AUDIO", "libSceAudio3d subsystem initialized.");
    return 0;
}

PS4_SYSV_ABI s32 sceAudio3dGetDefaultOpenParameters(void* params) {
    if (params) {
        std::memset(params, 0, 64);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceAudio3dPortOpen(s32 type, const void* params, s32* port_id) {
    (void)params;
    s32 port = s_next_audio3d_port++;
    if (port_id) {
        *port_id = port;
    }
    LOG_INFO("AUDIO", "sceAudio3dPortOpen (type=" + std::to_string(type) + ") -> port " + std::to_string(port));
    return 0;
}

PS4_SYSV_ABI s32 sceAudio3dPortSetAttribute(s32 port, const void* attr) {
    (void)port;
    (void)attr;
    return 0;
}

// libSceNgs2
static u8 s_dummy_ngs2_sys[256]{};
static u8 s_dummy_ngs2_rack[256]{};
static std::atomic<u32> s_next_voice_handle{1};

PS4_SYSV_ABI s32 sceNgs2SystemCreateWithAllocator(const void* desc, const void* alloc, void** handle) {
    (void)desc;
    (void)alloc;
    if (handle) {
        *handle = s_dummy_ngs2_sys;
    }
    LOG_INFO("AUDIO", "sceNgs2SystemCreateWithAllocator called");
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2RackCreateWithAllocator(void* sys, const void* desc, const void* alloc, void** handle) {
    (void)sys;
    (void)desc;
    (void)alloc;
    if (handle) {
        *handle = s_dummy_ngs2_rack;
    }
    LOG_INFO("AUDIO", "sceNgs2RackCreateWithAllocator called");
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2RackGetVoiceHandle(void* rack, u32 voice_index, void** voice_handle) {
    (void)rack;
    (void)voice_index;
    u32 v = (s_next_voice_handle.fetch_add(1) % 64) + 1;
    if (voice_handle) {
        *voice_handle = reinterpret_cast<void*>(static_cast<uintptr_t>(v));
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2VoiceControl(void* voice, const void* params) {
    (void)voice;
    (void)params;
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2VoiceGetState(void* voice, void* state) {
    (void)voice;
    if (state) {
        *reinterpret_cast<u32*>(state) = 1; // playing / idle
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNgs2RackQueryBufferSize(const void* desc, usize* buffer_size) {
    (void)desc;
    if (buffer_size) {
        *buffer_size = 0x40000; // 256 KB safe buffer
    }
    return 0;
}

} // extern "C"

} // namespace Audio
