#include "kernel/dialog_service.h"
#include "common/logger.h"
#include <cstring>

namespace Dialog {

static s32 s_msg_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
static s32 s_error_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
static s32 s_signin_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
static s32 s_playgo_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;

extern "C" {

// ========================================================================
// libSceCommonDialog
// ========================================================================

PS4_SYSV_ABI s32 sceCommonDialogInitialize(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceCommonDialogInitialize called");
    return 0;
}

// ========================================================================
// libSceMsgDialog
// ========================================================================

PS4_SYSV_ABI s32 sceMsgDialogInitialize() {
    s_msg_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceMsgDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceMsgDialogOpen(const void* param) {
    (void)param;
    s_msg_dialog_status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    LOG_INFO("DIALOG", "sceMsgDialogOpen called -> status set to FINISHED");
    return 0;
}

PS4_SYSV_ABI s32 sceMsgDialogGetStatus() {
    return s_msg_dialog_status == SCE_COMMON_DIALOG_STATUS_NONE ? SCE_COMMON_DIALOG_STATUS_FINISHED : s_msg_dialog_status;
}

PS4_SYSV_ABI s32 sceMsgDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    LOG_INFO("DIALOG", "sceMsgDialogGetResult called -> returned 0 (SCE_OK)");
    return 0;
}

PS4_SYSV_ABI s32 sceMsgDialogClose() {
    s_msg_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceMsgDialogClose called");
    return 0;
}

PS4_SYSV_ABI s32 sceMsgDialogTerminate() {
    s_msg_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceMsgDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceMsgDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceErrorDialog
// ========================================================================

PS4_SYSV_ABI s32 sceErrorDialogInitialize() {
    s_error_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceErrorDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceErrorDialogOpen(const void* param) {
    (void)param;
    s_error_dialog_status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    LOG_INFO("DIALOG", "sceErrorDialogOpen called -> status set to FINISHED");
    return 0;
}

PS4_SYSV_ABI s32 sceErrorDialogGetStatus() {
    return s_error_dialog_status == SCE_COMMON_DIALOG_STATUS_NONE ? SCE_COMMON_DIALOG_STATUS_FINISHED : s_error_dialog_status;
}

PS4_SYSV_ABI s32 sceErrorDialogTerminate() {
    s_error_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceErrorDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceErrorDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceSigninDialog
// ========================================================================

PS4_SYSV_ABI s32 sceSigninDialogInitialize() {
    s_signin_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceSigninDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceSigninDialogOpen(const void* param) {
    (void)param;
    s_signin_dialog_status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    LOG_INFO("DIALOG", "sceSigninDialogOpen called -> status set to FINISHED");
    return 0;
}

PS4_SYSV_ABI s32 sceSigninDialogGetStatus() {
    return s_signin_dialog_status == SCE_COMMON_DIALOG_STATUS_NONE ? SCE_COMMON_DIALOG_STATUS_FINISHED : s_signin_dialog_status;
}

PS4_SYSV_ABI s32 sceSigninDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    LOG_INFO("DIALOG", "sceSigninDialogGetResult called -> returned 0 (SCE_OK)");
    return 0;
}

PS4_SYSV_ABI s32 sceSigninDialogClose() {
    s_signin_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceSigninDialogClose called");
    return 0;
}

PS4_SYSV_ABI s32 sceSigninDialogTerminate() {
    s_signin_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "sceSigninDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceSigninDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libScePlayGoDialog
// ========================================================================

PS4_SYSV_ABI s32 scePlayGoDialogInitialize() {
    s_playgo_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "scePlayGoDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 scePlayGoDialogOpen(const void* param) {
    (void)param;
    s_playgo_dialog_status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    LOG_INFO("DIALOG", "scePlayGoDialogOpen called -> status set to FINISHED");
    return 0;
}

PS4_SYSV_ABI s32 scePlayGoDialogGetStatus() {
    return s_playgo_dialog_status == SCE_COMMON_DIALOG_STATUS_NONE ? SCE_COMMON_DIALOG_STATUS_FINISHED : s_playgo_dialog_status;
}

PS4_SYSV_ABI s32 scePlayGoDialogClose() {
    s_playgo_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "scePlayGoDialogClose called");
    return 0;
}

PS4_SYSV_ABI s32 scePlayGoDialogTerminate() {
    s_playgo_dialog_status = SCE_COMMON_DIALOG_STATUS_NONE;
    LOG_INFO("DIALOG", "scePlayGoDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 scePlayGoDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

} // extern "C"

} // namespace Dialog
