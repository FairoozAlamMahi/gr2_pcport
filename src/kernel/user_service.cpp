#include "kernel/user_service.h"
#include "common/logger.h"
#include <cstring>
#include <atomic>

namespace Kernel::User {

extern "C" {

PS4_SYSV_ABI s32 sceUserServiceInitialize(const void* param) {
    (void)param;
    LOG_INFO("USER", "sceUserServiceInitialize called successfully");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceTerminate() {
    LOG_INFO("USER", "sceUserServiceTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceGetInitialUser(s32* user_id) {
    if (user_id) {
        *user_id = 1; // Default primary user ID
    }
    LOG_INFO("USER", "sceUserServiceGetInitialUser returned user ID 1");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceGetLoginUserIdList(void* id_list) {
    if (id_list) {
        s32* ids = reinterpret_cast<s32*>(id_list);
        ids[0] = 1;  // Primary user ID 1
        ids[1] = -1; // SCE_USER_SERVICE_USER_ID_INVALID
        ids[2] = -1;
        ids[3] = -1;
    }
    LOG_INFO("USER", "sceUserServiceGetLoginUserIdList returned 1 active user (ID=1)");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceGetUserName(s32 user_id, char* name, usize max_len) {
    (void)user_id;
    if (name && max_len > 0) {
        strncpy_s(name, max_len, "KatPC", _TRUNCATE);
    }
    LOG_INFO("USER", "sceUserServiceGetUserName returned 'KatPC'");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceRegisterEventCallback(const void* callback, void* user_data) {
    (void)callback;
    (void)user_data;
    LOG_INFO("USER", "sceUserServiceRegisterEventCallback registered callback");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceUnregisterEventCallback(const void* callback) {
    (void)callback;
    LOG_INFO("USER", "sceUserServiceUnregisterEventCallback unregistered callback");
    return 0;
}

PS4_SYSV_ABI s32 sceUserServiceGetEvent(void* event) {
    static std::atomic<bool> s_login_dispatched{false};
    if (!s_login_dispatched.exchange(true)) {
        if (event) {
            std::memset(event, 0, 32);
            u32* ev = reinterpret_cast<u32*>(event);
            ev[0] = 0; // SCE_USER_SERVICE_EVENT_TYPE_LOGIN
            ev[1] = 1; // SceUserServiceUserId = 1
        }
        LOG_INFO("USER", "sceUserServiceGetEvent dispatched SCE_USER_SERVICE_EVENT_TYPE_LOGIN for user ID 1");
        return 0; // SCE_OK
    }
    if (event) {
        std::memset(event, 0, 32);
    }
    // Return SCE_USER_SERVICE_ERROR_NO_EVENT (0x80960001) so the engine's event drain while-loop completes
    return static_cast<s32>(0x80960001u);
}

} // extern "C"

} // namespace Kernel::User
