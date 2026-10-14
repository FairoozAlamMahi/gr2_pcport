#include "kernel/rtc_service.h"
#include <chrono>
#include <cstring>

namespace Kernel::Rtc {

extern "C" {

PS4_SYSV_ABI s32 sceRtcGetCurrentNetworkTick(u64* tick) {
    if (tick) {
        *tick = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcGetTick(const void* time, u64* tick) {
    (void)time;
    if (tick) {
        *tick = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcTickAddDays(u64* tick, const u64* src, s32 days) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(days) * 86400000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcTickAddHours(u64* tick, const u64* src, s32 hours) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(hours) * 3600000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcTickAddMinutes(u64* tick, const u64* src, s32 minutes) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(minutes) * 60000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcTickAddSeconds(u64* tick, const u64* src, s64 seconds) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(seconds) * 1000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcConvertUtcToLocalTime(const void* utc, void* local) {
    if (local && utc) {
        std::memcpy(local, utc, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcConvertLocalTimeToUtc(const void* local, void* utc) {
    if (utc && local) {
        std::memcpy(utc, local, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcFormatRfc3339(char* str, const void* time, s32 timezone) {
    (void)time;
    (void)timezone;
    if (str) {
        std::memcpy(str, "2026-10-14T21:30:00Z\0", 21);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcFormatRFC3339(char* str, const void* time, s32 timezone) {
    return sceRtcFormatRfc3339(str, time, timezone);
}

PS4_SYSV_ABI s32 sceRtcParseRfc3339(void* time, const char* str) {
    (void)time;
    (void)str;
    return 0;
}

PS4_SYSV_ABI s32 sceRtcParseRFC3339(void* time, const char* str) {
    return sceRtcParseRfc3339(time, str);
}

PS4_SYSV_ABI s32 sceRtcParseDateTime(void* time, const char* str) {
    (void)time;
    (void)str;
    return 0;
}

PS4_SYSV_ABI s32 sceRtcGetTickResolution(u64* res) {
    if (res) {
        *res = 1000000; // 1,000,000 ticks per second (microsecond resolution)
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcFormatRFC2822(char* str, const void* time, s32 timezone) {
    (void)time;
    (void)timezone;
    if (str) {
        std::memcpy(str, "Wed, 14 Oct 2026 21:30:00 GMT\0", 30);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcFormatRFC2822LocalTime(char* str, const void* time) {
    return sceRtcFormatRFC2822(str, time, 0);
}

PS4_SYSV_ABI s32 sceRtcTickAddMonths(u64* tick, const u64* src, s32 months) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(months) * 30ULL * 86400000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcTickAddYears(u64* tick, const u64* src, s32 years) {
    if (tick && src) {
        *tick = *src + static_cast<u64>(years) * 365ULL * 86400000000ULL;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcGetTime_t(const void* time, int64_t* t) {
    (void)time;
    if (t) {
        *t = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return 0;
}

PS4_SYSV_ABI s32 sceRtcSetTick(void* time, const u64* tick) {
    (void)time;
    (void)tick;
    return 0;
}

} // extern "C"

} // namespace Kernel::Rtc
