#include "kernel/np_service.h"
#include "common/logger.h"
#include <cstring>
#include <string>
#include <atomic>

namespace Np {

extern "C" {

// ============================================================================
// libSceNpManager
// ============================================================================

PS4_SYSV_ABI s32 sceNpManagerGetStatus(s32* status) {
    if (status) {
        *status = 1; // 1 = OFFLINE / SIGNED_OUT (SCE_NP_STATE_SIGNED_OUT)
    }
    LOG_INFO("NP", "sceNpManagerGetStatus: status = 1 (offline/signed_out)");
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetState(void* arg0, void* arg1) {
    if (arg1) {
        *static_cast<s32*>(arg1) = 1;
    } else if (arg0) {
        *static_cast<s32*>(arg0) = 1;
    }
    LOG_INFO("NP", "sceNpGetState: state = 1 (offline/signed_out)");
    return 0;
}

PS4_SYSV_ABI s32 sceNpManagerGetOnlineId(void* online_id) {
    if (online_id) {
        std::memset(online_id, 0, 16);
        std::memcpy(online_id, "KatPC\0", 6);
    }
    LOG_INFO("NP", "sceNpManagerGetOnlineId: onlineId = 'KatPC'");
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetOnlineId(void* arg0, void* arg1) {
    void* dest = arg1 ? arg1 : arg0;
    if (dest) {
        std::memset(dest, 0, 16);
        std::memcpy(dest, "KatPC\0", 6);
    }
    LOG_INFO("NP", "sceNpGetOnlineId: onlineId = 'KatPC'");
    return 0;
}

PS4_SYSV_ABI s32 sceNpManagerGetNpId(void* np_id) {
    if (np_id) {
        // SceNpId: handle[16], opt[8], reserved[8] (32 bytes)
        std::memset(np_id, 0, 32);
        std::memcpy(np_id, "KatPC\0", 6);
    }
    LOG_INFO("NP", "sceNpManagerGetNpId: npId = 'KatPC'");
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetNpId(void* arg0, void* arg1) {
    void* dest = arg1 ? arg1 : arg0;
    if (dest) {
        std::memset(dest, 0, 32);
        std::memcpy(dest, "KatPC\0", 6);
    }
    LOG_INFO("NP", "sceNpGetNpId: npId = 'KatPC'");
    return 0;
}

PS4_SYSV_ABI s32 sceNpManagerGetAccountId(void* account_id) {
    if (account_id) {
        *static_cast<u64*>(account_id) = 0x10001ULL;
    }
    LOG_INFO("NP", "sceNpManagerGetAccountId: accountId = 0x10001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetAccountId(void* arg0, void* arg1) {
    void* dest = arg1 ? arg1 : arg0;
    if (dest) {
        *static_cast<u64*>(dest) = 0x10001ULL;
    }
    LOG_INFO("NP", "sceNpGetAccountId: accountId = 0x10001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpCheckCallback() {
    return 0;
}

PS4_SYSV_ABI s32 sceNpCheckCallbackForLib() {
    return 0;
}

static std::atomic<s32> s_next_np_req_id{1};

PS4_SYSV_ABI s32 sceNpRegisterNpReachabilityStateCallback(const void* cb, void* arg) {
    (void)cb;
    (void)arg;
    LOG_INFO("NP", "sceNpRegisterNpReachabilityStateCallback: reachability callback registered");
    return 0;
}

PS4_SYSV_ABI s32 sceNpRegisterStateCallbackForToolkit(const void* cb, void* arg) {
    (void)cb;
    (void)arg;
    LOG_INFO("NP", "sceNpRegisterStateCallbackForToolkit: toolkit state callback registered");
    return 0;
}

PS4_SYSV_ABI s32 sceNpUnregisterStateCallbackForToolkit(const void* cb) {
    (void)cb;
    LOG_INFO("NP", "sceNpUnregisterStateCallbackForToolkit: toolkit state callback unregistered");
    return 0;
}

PS4_SYSV_ABI s32 sceNpCreateAsyncRequest(const void* param, s32* req_id) {
    (void)param;
    s32 id = s_next_np_req_id.fetch_add(1, std::memory_order_relaxed);
    if (req_id) {
        *req_id = id;
    }
    LOG_INFO("NP", "sceNpCreateAsyncRequest: created async req ID " + std::to_string(id));
    return 0;
}

PS4_SYSV_ABI s32 sceNpCheckNpAvailability(s32 req_id) {
    LOG_INFO("NP", "sceNpCheckNpAvailability: req ID " + std::to_string(req_id) + " -> available");
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetParentalControlInfo(s32 req_id, void* info) {
    uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(caller));
    if (info) {
        // SceNpParentalControlInfo is 8 bytes (chatRestriction=0, ugcRestriction=0)
        std::memset(info, 0, 8);
    }
    LOG_INFO("NP", std::string("sceNpGetParentalControlInfo called from ") + buf +
                   ": req ID " + std::to_string(req_id) + " -> unrestricted");
    return 0;
}

PS4_SYSV_ABI s32 sceNpPollAsync(s32 req_id, s32* result) {
    if (result) {
        *result = 0; // Completed successfully (SCE_OK)
    }
    LOG_INFO("NP", "sceNpPollAsync: req ID " + std::to_string(req_id) + " -> completed (0)");
    return 0;
}

PS4_SYSV_ABI s32 sceNpDeleteRequest(s32 req_id) {
    LOG_INFO("NP", "sceNpDeleteRequest: req ID " + std::to_string(req_id));
    return 0;
}

// ============================================================================
// libSceNpTrophy
// ============================================================================

PS4_SYSV_ABI s32 sceNpTrophyInit(const void* opt) {
    (void)opt;
    LOG_INFO("NP", "sceNpTrophyInit: offline trophy emulation initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyTerm() {
    LOG_INFO("NP", "sceNpTrophyTerm called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyCreateContext(void** ctx, const void* np_title_id, const void* np_title_secret, u64 opt) {
    (void)np_title_id;
    (void)np_title_secret;
    (void)opt;
    if (ctx) {
        *reinterpret_cast<uintptr_t*>(ctx) = 0x1001;
    }
    LOG_INFO("NP", "sceNpTrophyCreateContext: created offline context handle 0x1001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyDestroyContext(void* ctx) {
    (void)ctx;
    LOG_INFO("NP", "sceNpTrophyDestroyContext called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyCreateHandle(void** handle) {
    if (handle) {
        *reinterpret_cast<uintptr_t*>(handle) = 0x2001;
    }
    LOG_INFO("NP", "sceNpTrophyCreateHandle: created offline trophy handle 0x2001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyDestroyHandle(void* handle) {
    (void)handle;
    LOG_INFO("NP", "sceNpTrophyDestroyHandle called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyRegisterContext(void* ctx, void* handle, const void* cb, void* user_data, u64 opt) {
    (void)ctx;
    (void)handle;
    (void)cb;
    (void)user_data;
    (void)opt;
    LOG_INFO("NP", "sceNpTrophyRegisterContext: context registered successfully");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyUnlockTrophy(void* ctx, void* handle, u32 trophy_id, void* result) {
    (void)ctx;
    (void)handle;
    if (result) {
        *static_cast<s32*>(result) = 0;
    }
    LOG_INFO("NP", "Trophy unlocked: ID " + std::to_string(trophy_id));
    return 0;
}

PS4_SYSV_ABI s32 sceNpTrophyGetTrophyUnlockState(void* ctx, void* handle, void* state, u32* count) {
    (void)ctx;
    (void)handle;
    if (state) {
        std::memset(state, 0, 128);
    }
    if (count) {
        *count = 0;
    }
    return 0;
}

// ============================================================================
// libSceNpMatching2
// ============================================================================

PS4_SYSV_ABI s32 sceNpMatching2Initialize(const void* opt) {
    (void)opt;
    LOG_INFO("NP", "sceNpMatching2Initialize: offline matching initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceNpMatching2Terminate() {
    LOG_INFO("NP", "sceNpMatching2Terminate called");
    return 0;
}

// ============================================================================
// libSceNpScoreRanking
// ============================================================================

PS4_SYSV_ABI s32 sceNpScoreRankingInitialize(const void* opt) {
    (void)opt;
    LOG_INFO("NP", "sceNpScoreRankingInitialize: offline ranking initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceNpScoreRankingTerminate() {
    LOG_INFO("NP", "sceNpScoreRankingTerminate called");
    return 0;
}

// ============================================================================
// libSceNpTus
// ============================================================================

PS4_SYSV_ABI s32 sceNpTusCreateNpTitleCtx(const void* np_title_id, void** title_ctx) {
    (void)np_title_id;
    if (title_ctx) {
        *title_ctx = reinterpret_cast<void*>(0x3001);
    }
    LOG_INFO("NP", "sceNpTusCreateNpTitleCtx called -> handle 0x3001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusDeleteNpTitleCtx(void* title_ctx) {
    (void)title_ctx;
    LOG_INFO("NP", "sceNpTusDeleteNpTitleCtx called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusCreateRequest(void* title_ctx, void** req) {
    (void)title_ctx;
    if (req) {
        *req = reinterpret_cast<void*>(0x4001);
    }
    LOG_INFO("NP", "sceNpTusCreateRequest called -> handle 0x4001");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusDeleteRequest(void* req) {
    (void)req;
    LOG_INFO("NP", "sceNpTusDeleteRequest called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusSetData(void* req, const void* slot, const void* data, usize size, void* out) {
    (void)req;
    (void)slot;
    (void)data;
    (void)size;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusSetDataVUser(void* req, const void* vuser, const void* slot, const void* data, usize size, void* out) {
    (void)req;
    (void)vuser;
    (void)slot;
    (void)data;
    (void)size;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusGetData(void* req, const void* slot, void* buf, usize size, void* out) {
    (void)req;
    (void)slot;
    if (buf && size > 0) {
        std::memset(buf, 0, size);
    }
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusGetDataVUser(void* req, const void* vuser, const void* slot, void* buf, usize size, void* out) {
    (void)req;
    (void)vuser;
    (void)slot;
    if (buf && size > 0) {
        std::memset(buf, 0, size);
    }
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusSetMultiSlotVariable(void* req, const void* slots, u32 count, void* out) {
    (void)req;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusSetMultiSlotVariableVUser(void* req, const void* vuser, const void* slots, u32 count, void* out) {
    (void)req;
    (void)vuser;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusGetMultiSlotVariable(void* req, const void* slots, u32 count, void* out) {
    (void)req;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusGetMultiSlotVariableVUser(void* req, const void* vuser, const void* slots, u32 count, void* out) {
    (void)req;
    (void)vuser;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusAddAndGetVariable(void* req, const void* slot, s64 delta, void* out) {
    (void)req;
    (void)slot;
    (void)delta;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusAddAndGetVariableVUser(void* req, const void* vuser, const void* slot, s64 delta, void* out) {
    (void)req;
    (void)vuser;
    (void)slot;
    (void)delta;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusDeleteMultiSlotData(void* req, const void* slots, u32 count, void* out) {
    (void)req;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTusDeleteMultiSlotDataVUser(void* req, const void* vuser, const void* slots, u32 count, void* out) {
    (void)req;
    (void)vuser;
    (void)slots;
    (void)count;
    (void)out;
    return 0;
}

PS4_SYSV_ABI s32 sceNpTssGetData(void* req, const void* slot, void* buf, usize size, void* out) {
    (void)req;
    (void)slot;
    if (buf && size > 0) {
        std::memset(buf, 0, size);
    }
    (void)out;
    return 0;
}

// ============================================================================
// libSceNpWordFilter
// ============================================================================

PS4_SYSV_ABI s32 sceNpWordFilterAbortRequest(void* req) {
    (void)req;
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterCensorComment(void* req, const char* src, char* dst, usize size) {
    (void)req;
    if (src && dst && size > 0) {
        strncpy(dst, src, size - 1);
        dst[size - 1] = '\0';
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterCreateAsyncRequest(void* title_ctx, void** req) {
    (void)title_ctx;
    if (req) {
        *req = reinterpret_cast<void*>(0x5001);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterCreateRequest(void* title_ctx, void** req) {
    (void)title_ctx;
    if (req) {
        *req = reinterpret_cast<void*>(0x5002);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterCreateTitleCtx(const void* np_title_id, void** title_ctx) {
    (void)np_title_id;
    if (title_ctx) {
        *title_ctx = reinterpret_cast<void*>(0x5003);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterDeleteRequest(void* req) {
    (void)req;
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterDeleteTitleCtx(void* title_ctx) {
    (void)title_ctx;
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterPollAsync(void* req, s32* result) {
    (void)req;
    if (result) {
        *result = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWordFilterSanitizeComment(void* req, const char* src, char* dst, usize size) {
    (void)req;
    if (src && dst && size > 0) {
        strncpy(dst, src, size - 1);
        dst[size - 1] = '\0';
    }
    return 0;
}

// ============================================================================
// libSceNpLookup
// ============================================================================

PS4_SYSV_ABI s32 sceNpLookupCreateRequest(void* title_ctx, void** req) {
    (void)title_ctx;
    if (req) {
        *req = reinterpret_cast<void*>(0x6001);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpLookupCreateTitleCtx(const void* np_title_id, void** title_ctx) {
    (void)np_title_id;
    if (title_ctx) {
        *title_ctx = reinterpret_cast<void*>(0x6002);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpLookupDeleteRequest(void* req) {
    (void)req;
    return 0;
}

PS4_SYSV_ABI s32 sceNpLookupDeleteTitleCtx(void* title_ctx) {
    (void)title_ctx;
    return 0;
}

PS4_SYSV_ABI s32 sceNpLookupNpId(void* req, const void* online_id, void* np_id) {
    (void)req;
    (void)online_id;
    if (np_id) {
        std::memset(np_id, 0, 32);
        std::memcpy(np_id, "KatPC\0", 6);
    }
    return 0;
}

// ============================================================================
// libSceNpAuth & Utils
// ============================================================================

PS4_SYSV_ABI s32 sceNpAuthCreateRequest(const void* param, void** req) {
    (void)param;
    if (req) {
        *req = reinterpret_cast<void*>(0x7001);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpAuthDeleteRequest(void* req) {
    (void)req;
    return 0;
}

PS4_SYSV_ABI s32 sceNpAuthGetAuthorizationCode(void* req, const void* param, void* code) {
    (void)req;
    (void)param;
    if (code) {
        std::memset(code, 0, 64);
        std::memcpy(code, "AUTH_KAT_PC_PORT_CODE\0", 23);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpCmpNpId(const void* id1, const void* id2) {
    if (!id1 || !id2) return -1;
    return std::memcmp(id1, id2, 32);
}

PS4_SYSV_ABI s32 sceNpCmpOnlineId(const void* id1, const void* id2) {
    if (!id1 || !id2) return -1;
    return std::strncmp(reinterpret_cast<const char*>(id1), reinterpret_cast<const char*>(id2), 16);
}

PS4_SYSV_ABI s32 sceNpCreateRequest(const void* param, void** req) {
    (void)param;
    if (req) {
        *req = reinterpret_cast<void*>(0x8001);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetAccountCountry(void* req, void* country) {
    (void)req;
    if (country) {
        std::memcpy(country, "us\0", 3);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetAccountLanguage(void* req, void* lang) {
    (void)req;
    if (lang) {
        std::memcpy(lang, "en\0", 3);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookCreateRequest(const void* param, void** req) {
    (void)param;
    if (req) {
        *req = reinterpret_cast<void*>(0x9001);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookDeleteRequest(void* req) {
    (void)req;
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookGetAccessToken(void* req, void* token) {
    (void)req;
    if (token) {
        std::memset(token, 0, 64);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpWaitAsync(s32 req_id, s32* result) {
    (void)req_id;
    if (result) *result = 0;
    return 0;
}

PS4_SYSV_ABI s32 sceNpUnregisterPlusEventCallback(s32 id) {
    (void)id;
    return 0;
}

PS4_SYSV_ABI s32 sceNpUnregisterNpReachabilityStateCallback(s32 id) {
    (void)id;
    return 0;
}

PS4_SYSV_ABI s32 sceNpAbortRequest(s32 req_id) {
    (void)req_id;
    return 0;
}

PS4_SYSV_ABI s32 sceNpGetUserIdByOnlineId(const void* online_id, void* user_id) {
    (void)online_id;
    if (user_id) {
        *reinterpret_cast<u32*>(user_id) = 0x10000001;
    }
    return 0;
}

} // extern "C"

} // namespace Np
