#pragma once

#include "common/types.h"

namespace Fiber {

extern "C" {

PS4_SYSV_ABI s32 _sceFiberInitializeImpl(void* fiber, const char* name, void* entry, u64 argOnInitialize, void* addrContext, u64 sizeContext, const void* params);
PS4_SYSV_ABI s32 sceFiberRun(void* fiber, u64 argOnRun, u64* argOnReturn);
PS4_SYSV_ABI s32 sceFiberSwitch(void* fiber, u64 argOnRun, u64* argOnReturn);
PS4_SYSV_ABI s32 sceFiberReturnToThread(u64 argOnReturn, const void* params);
PS4_SYSV_ABI s32 sceFiberFinalize(void* fiber);

} // extern "C"

} // namespace Fiber
