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
PS4_SYSV_ABI s32 sceNpRegisterNpReachabilityStateCallback(const void* cb, void* arg);
PS4_SYSV_ABI s32 sceNpRegisterStateCallbackForToolkit(const void* cb, void* arg);
PS4_SYSV_ABI s32 sceNpUnregisterStateCallbackForToolkit(const void* cb);
PS4_SYSV_ABI s32 sceNpCreateAsyncRequest(const void* param, s32* req_id);
PS4_SYSV_ABI s32 sceNpCheckNpAvailability(s32 req_id);
PS4_SYSV_ABI s32 sceNpGetParentalControlInfo(s32 req_id, void* info);
PS4_SYSV_ABI s32 sceNpPollAsync(s32 req_id, s32* result);
PS4_SYSV_ABI s32 sceNpDeleteRequest(s32 req_id);

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

// libSceNpTus
PS4_SYSV_ABI s32 sceNpTusCreateNpTitleCtx(const void* np_title_id, void** title_ctx);
PS4_SYSV_ABI s32 sceNpTusDeleteNpTitleCtx(void* title_ctx);
PS4_SYSV_ABI s32 sceNpTusCreateRequest(void* title_ctx, void** req);
PS4_SYSV_ABI s32 sceNpTusDeleteRequest(void* req);
PS4_SYSV_ABI s32 sceNpTusSetData(void* req, const void* slot, const void* data, usize size, void* out);
PS4_SYSV_ABI s32 sceNpTusSetDataVUser(void* req, const void* vuser, const void* slot, const void* data, usize size, void* out);
PS4_SYSV_ABI s32 sceNpTusGetData(void* req, const void* slot, void* buf, usize size, void* out);
PS4_SYSV_ABI s32 sceNpTusGetDataVUser(void* req, const void* vuser, const void* slot, void* buf, usize size, void* out);
PS4_SYSV_ABI s32 sceNpTusSetMultiSlotVariable(void* req, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTusSetMultiSlotVariableVUser(void* req, const void* vuser, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTusGetMultiSlotVariable(void* req, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTusGetMultiSlotVariableVUser(void* req, const void* vuser, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTusAddAndGetVariable(void* req, const void* slot, s64 delta, void* out);
PS4_SYSV_ABI s32 sceNpTusAddAndGetVariableVUser(void* req, const void* vuser, const void* slot, s64 delta, void* out);
PS4_SYSV_ABI s32 sceNpTusDeleteMultiSlotData(void* req, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTusDeleteMultiSlotDataVUser(void* req, const void* vuser, const void* slots, u32 count, void* out);
PS4_SYSV_ABI s32 sceNpTssGetData(void* req, const void* slot, void* buf, usize size, void* out);

// libSceNpWordFilter
PS4_SYSV_ABI s32 sceNpWordFilterAbortRequest(void* req);
PS4_SYSV_ABI s32 sceNpWordFilterCensorComment(void* req, const char* src, char* dst, usize size);
PS4_SYSV_ABI s32 sceNpWordFilterCreateAsyncRequest(void* title_ctx, void** req);
PS4_SYSV_ABI s32 sceNpWordFilterCreateRequest(void* title_ctx, void** req);
PS4_SYSV_ABI s32 sceNpWordFilterCreateTitleCtx(const void* np_title_id, void** title_ctx);
PS4_SYSV_ABI s32 sceNpWordFilterDeleteRequest(void* req);
PS4_SYSV_ABI s32 sceNpWordFilterDeleteTitleCtx(void* title_ctx);
PS4_SYSV_ABI s32 sceNpWordFilterPollAsync(void* req, s32* result);
PS4_SYSV_ABI s32 sceNpWordFilterSanitizeComment(void* req, const char* src, char* dst, usize size);

// libSceNpLookup
PS4_SYSV_ABI s32 sceNpLookupCreateRequest(void* title_ctx, void** req);
PS4_SYSV_ABI s32 sceNpLookupCreateTitleCtx(const void* np_title_id, void** title_ctx);
PS4_SYSV_ABI s32 sceNpLookupDeleteRequest(void* req);
PS4_SYSV_ABI s32 sceNpLookupDeleteTitleCtx(void* title_ctx);
PS4_SYSV_ABI s32 sceNpLookupNpId(void* req, const void* online_id, void* np_id);

// libSceNpAuth & Utils
PS4_SYSV_ABI s32 sceNpAuthCreateRequest(const void* param, void** req);
PS4_SYSV_ABI s32 sceNpAuthDeleteRequest(void* req);
PS4_SYSV_ABI s32 sceNpAuthGetAuthorizationCode(void* req, const void* param, void* code);
PS4_SYSV_ABI s32 sceNpCmpNpId(const void* id1, const void* id2);
PS4_SYSV_ABI s32 sceNpCmpOnlineId(const void* id1, const void* id2);
PS4_SYSV_ABI s32 sceNpCreateRequest(const void* param, void** req);
PS4_SYSV_ABI s32 sceNpGetAccountCountry(void* req, void* country);
PS4_SYSV_ABI s32 sceNpGetAccountLanguage(void* req, void* lang);
PS4_SYSV_ABI s32 sceNpSnsFacebookCreateRequest(const void* param, void** req);
PS4_SYSV_ABI s32 sceNpSnsFacebookDeleteRequest(void* req);
PS4_SYSV_ABI s32 sceNpSnsFacebookGetAccessToken(void* req, void* token);

} // extern "C"

} // namespace Np
