#include "video/avplayer_service.h"
#include "common/logger.h"

namespace Video::AvPlayer {

extern "C" {

PS4_SYSV_ABI void* sceAvPlayerInit(const void* initParam) {
    (void)initParam;
    LOG_INFO("AVPLAYER", "sceAvPlayerInit: initialized video playback subsystem (dummy handle 0x1000)");
    return reinterpret_cast<void*>(0x1000);
}

PS4_SYSV_ABI s32 sceAvPlayerPostInit(void* handle, const void* param) {
    (void)handle;
    (void)param;
    LOG_INFO("AVPLAYER", "sceAvPlayerPostInit called");
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerClose(void* handle) {
    (void)handle;
    LOG_INFO("AVPLAYER", "sceAvPlayerClose called");
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerAddSource(void* handle, const char* filename) {
    (void)handle;
    std::string path = filename ? filename : "null";
    LOG_INFO("AVPLAYER", "sceAvPlayerAddSource: video path = '" + path + "'");
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerStart(void* handle) {
    (void)handle;
    LOG_INFO("AVPLAYER", "sceAvPlayerStart called");
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerStop(void* handle) {
    (void)handle;
    LOG_INFO("AVPLAYER", "sceAvPlayerStop called");
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerPause(void* handle) {
    (void)handle;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerResume(void* handle) {
    (void)handle;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerIsActive(void* handle) {
    (void)handle;
    // Reporting 0 allows the game to immediately transition to the Title Screen / Main Menu without hanging on video codecs
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerEnableStream(void* handle, u32 stream_id) {
    (void)handle;
    (void)stream_id;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerStreamCount(void* handle) {
    (void)handle;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerGetStreamInfo(void* handle, u32 stream_id, void* info) {
    (void)handle;
    (void)stream_id;
    (void)info;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerGetVideoDataEx(void* handle, void* data) {
    (void)handle;
    (void)data;
    return 0;
}

PS4_SYSV_ABI s32 sceAvPlayerSetLooping(void* handle, bool loop) {
    (void)handle;
    (void)loop;
    return 0;
}

} // extern "C"

} // namespace Video::AvPlayer
