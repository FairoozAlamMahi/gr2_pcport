#pragma once

#include "common/types.h"

namespace Kernel::User {

extern "C" {

PS4_SYSV_ABI s32 sceUserServiceInitialize(const void* param = nullptr);
PS4_SYSV_ABI s32 sceUserServiceTerminate();
PS4_SYSV_ABI s32 sceUserServiceGetInitialUser(s32* user_id);
PS4_SYSV_ABI s32 sceUserServiceGetLoginUserIdList(void* id_list);
PS4_SYSV_ABI s32 sceUserServiceGetUserName(s32 user_id, char* name, usize max_len);
PS4_SYSV_ABI s32 sceUserServiceRegisterEventCallback(const void* callback = nullptr, void* user_data = nullptr);
PS4_SYSV_ABI s32 sceUserServiceUnregisterEventCallback(const void* callback = nullptr);
PS4_SYSV_ABI s32 sceUserServiceGetEvent(void* event = nullptr);

} // extern "C"

} // namespace Kernel::User
