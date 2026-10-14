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

// ========================================================================
// libSceGameCustomDataDialog
// ========================================================================

PS4_SYSV_ABI s32 sceGameCustomDataDialogInitialize(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceGameCustomDataDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceGameCustomDataDialogOpen(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceGameCustomDataDialogOpen called");
    return 0;
}

PS4_SYSV_ABI s32 sceGameCustomDataDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceGameCustomDataDialogTerminate() {
    LOG_INFO("DIALOG", "sceGameCustomDataDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceGameCustomDataDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceNpCommerceDialog
// ========================================================================

PS4_SYSV_ABI s32 sceNpCommerceDialogInitialize(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceNpCommerceDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpCommerceDialogOpen(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceNpCommerceDialogOpen called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpCommerceDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpCommerceDialogTerminate() {
    LOG_INFO("DIALOG", "sceNpCommerceDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpCommerceDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceInvitationDialog
// ========================================================================

PS4_SYSV_ABI s32 sceInvitationDialogInitialize(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceInvitationDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceInvitationDialogOpen(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceInvitationDialogOpen called");
    return 0;
}

PS4_SYSV_ABI s32 sceInvitationDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceInvitationDialogTerminate() {
    LOG_INFO("DIALOG", "sceInvitationDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceInvitationDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceNpSnsFacebookDialog
// ========================================================================

PS4_SYSV_ABI s32 sceNpSnsFacebookDialogInitialize(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceNpSnsFacebookDialogInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookDialogOpen(const void* param) {
    (void)param;
    LOG_INFO("DIALOG", "sceNpSnsFacebookDialogOpen called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookDialogGetResult(void* result) {
    if (result) {
        std::memset(result, 0, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookDialogTerminate() {
    LOG_INFO("DIALOG", "sceNpSnsFacebookDialogTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceNpSnsFacebookDialogUpdateStatus() {
    return SCE_COMMON_DIALOG_STATUS_FINISHED;
}

// ========================================================================
// libSceShareUtility
// ========================================================================

PS4_SYSV_ABI s32 sceShareUtilityOpenShareMenu(const void* param) {
    (void)param;
    LOG_INFO("SHARE", "sceShareUtilityOpenShareMenu called");
    return 0;
}

PS4_SYSV_ABI s32 sceShareUtilityOpenShareMenuDefault(const void* param) {
    (void)param;
    LOG_INFO("SHARE", "sceShareUtilityOpenShareMenuDefault called");
    return 0;
}

PS4_SYSV_ABI s32 sceShareUtilityTerminate() {
    LOG_INFO("SHARE", "sceShareUtilityTerminate called");
    return 0;
}

} // extern "C"

} // namespace Dialog
