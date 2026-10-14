#pragma once

#include "common/types.h"

namespace Np {

extern "C" {

// libSceNpManager
PS4_SYSV_ABI s32 sceNpManagerGetStatus(s32* status);
PS4_SYSV_ABI s32 sceNpGetState(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceNpManagerGetOnlineId(void* online_id);
PS4_SYSV_ABI s32 sceNpGetOnlineId(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceNpManagerGetNpId(void* np_id);
PS4_SYSV_ABI s32 sceNpGetNpId(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceNpManagerGetAccountId(void* account_id);
PS4_SYSV_ABI s32 sceNpGetAccountId(void* arg0, void* arg1);
PS4_SYSV_ABI s32 sceNpCheckCallback();
PS4_SYSV_ABI s32 sceNpCheckCallbackForLib();

// libSceNpTrophy
PS4_SYSV_ABI s32 sceNpTrophyInit(const void* opt);
PS4_SYSV_ABI s32 sceNpTrophyTerm();
PS4_SYSV_ABI s32 sceNpTrophyCreateContext(void** ctx, const void* np_title_id, const void* np_title_secret, u64 opt);
PS4_SYSV_ABI s32 sceNpTrophyDestroyContext(void* ctx);
PS4_SYSV_ABI s32 sceNpTrophyCreateHandle(void** handle);
PS4_SYSV_ABI s32 sceNpTrophyDestroyHandle(void* handle);
PS4_SYSV_ABI s32 sceNpTrophyRegisterContext(void* ctx, void* handle, const void* cb, void* user_data, u64 opt);
PS4_SYSV_ABI s32 sceNpTrophyUnlockTrophy(void* ctx, void* handle, u32 trophy_id, void* result);
PS4_SYSV_ABI s32 sceNpTrophyGetTrophyUnlockState(void* ctx, void* handle, void* state, u32* count);

// libSceNpMatching2
PS4_SYSV_ABI s32 sceNpMatching2Initialize(const void* opt);
PS4_SYSV_ABI s32 sceNpMatching2Terminate();

// libSceNpScoreRanking
PS4_SYSV_ABI s32 sceNpScoreRankingInitialize(const void* opt);
PS4_SYSV_ABI s32 sceNpScoreRankingTerminate();

} // extern "C"

} // namespace Np
