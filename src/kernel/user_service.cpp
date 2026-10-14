#include "kernel/user_service.h"
#include "common/logger.h"
#include <cstring>

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
        // Struct SceUserServiceLoginUserIdList:
        // Setting index 0 and 1 to 1 guarantees:
        // - if parsed as struct { u32 count; s32 userId[4]; }, count = 1 and userId[0] = 1
        // - if parsed as struct { s32 userId[4]; }, userId[0] = 1
        s32* ids = reinterpret_cast<s32*>(id_list);
        ids[0] = 1;
        ids[1] = 1;
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
    if (event) {
        std::memset(event, 0, 32);
    }
    // Return SCE_USER_SERVICE_ERROR_NO_EVENT (0x80960001) so the engine's event drain while-loop completes
    return static_cast<s32>(0x80960001u);
}

} // extern "C"

} // namespace Kernel::User
