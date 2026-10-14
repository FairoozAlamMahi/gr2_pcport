#pragma once

#include "common/types.h"

namespace Kernel::Rtc {

extern "C" {

PS4_SYSV_ABI s32 sceRtcGetCurrentNetworkTick(u64* tick);
PS4_SYSV_ABI s32 sceRtcGetTick(const void* time, u64* tick);
PS4_SYSV_ABI s32 sceRtcTickAddDays(u64* tick, const u64* src, s32 days);
PS4_SYSV_ABI s32 sceRtcTickAddHours(u64* tick, const u64* src, s32 hours);
PS4_SYSV_ABI s32 sceRtcTickAddMinutes(u64* tick, const u64* src, s32 minutes);
PS4_SYSV_ABI s32 sceRtcTickAddSeconds(u64* tick, const u64* src, s64 seconds);
PS4_SYSV_ABI s32 sceRtcConvertUtcToLocalTime(const void* utc, void* local);
PS4_SYSV_ABI s32 sceRtcConvertLocalTimeToUtc(const void* local, void* utc);
PS4_SYSV_ABI s32 sceRtcFormatRfc3339(char* str, const void* time, s32 timezone);
PS4_SYSV_ABI s32 sceRtcFormatRFC3339(char* str, const void* time, s32 timezone);
PS4_SYSV_ABI s32 sceRtcParseRfc3339(void* time, const char* str);
PS4_SYSV_ABI s32 sceRtcParseRFC3339(void* time, const char* str);
PS4_SYSV_ABI s32 sceRtcParseDateTime(void* time, const char* str);
PS4_SYSV_ABI s32 sceRtcGetTickResolution(u64* res);
PS4_SYSV_ABI s32 sceRtcFormatRFC2822(char* str, const void* time, s32 timezone);
PS4_SYSV_ABI s32 sceRtcFormatRFC2822LocalTime(char* str, const void* time);
PS4_SYSV_ABI s32 sceRtcTickAddMonths(u64* tick, const u64* src, s32 months);
PS4_SYSV_ABI s32 sceRtcTickAddYears(u64* tick, const u64* src, s32 years);
PS4_SYSV_ABI s32 sceRtcGetTime_t(const void* time, int64_t* t);
PS4_SYSV_ABI s32 sceRtcSetTick(void* time, const u64* tick);

} // extern "C"

} // namespace Kernel::Rtc

namespace Rtc = Kernel::Rtc;
