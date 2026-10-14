#pragma once

#include "common/types.h"

namespace Dialog {

constexpr s32 SCE_COMMON_DIALOG_STATUS_NONE     = 0;
constexpr s32 SCE_COMMON_DIALOG_STATUS_RUNNING  = 1;
constexpr s32 SCE_COMMON_DIALOG_STATUS_FINISHED = 2;

extern "C" {

// libSceCommonDialog
PS4_SYSV_ABI s32 sceCommonDialogInitialize(const void* param = nullptr);

// libSceMsgDialog
PS4_SYSV_ABI s32 sceMsgDialogInitialize();
PS4_SYSV_ABI s32 sceMsgDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceMsgDialogGetStatus();
PS4_SYSV_ABI s32 sceMsgDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceMsgDialogClose();
PS4_SYSV_ABI s32 sceMsgDialogTerminate();
PS4_SYSV_ABI s32 sceMsgDialogUpdateStatus();

// libSceErrorDialog
PS4_SYSV_ABI s32 sceErrorDialogInitialize();
PS4_SYSV_ABI s32 sceErrorDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceErrorDialogGetStatus();
PS4_SYSV_ABI s32 sceErrorDialogTerminate();
PS4_SYSV_ABI s32 sceErrorDialogUpdateStatus();

// libSceSigninDialog
PS4_SYSV_ABI s32 sceSigninDialogInitialize();
PS4_SYSV_ABI s32 sceSigninDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceSigninDialogGetStatus();
PS4_SYSV_ABI s32 sceSigninDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceSigninDialogClose();
PS4_SYSV_ABI s32 sceSigninDialogTerminate();
PS4_SYSV_ABI s32 sceSigninDialogUpdateStatus();

// libScePlayGoDialog
PS4_SYSV_ABI s32 scePlayGoDialogInitialize();
PS4_SYSV_ABI s32 scePlayGoDialogOpen(const void* param);
PS4_SYSV_ABI s32 scePlayGoDialogGetStatus();
PS4_SYSV_ABI s32 scePlayGoDialogClose();
PS4_SYSV_ABI s32 scePlayGoDialogTerminate();
PS4_SYSV_ABI s32 scePlayGoDialogUpdateStatus();

// libSceGameCustomDataDialog
PS4_SYSV_ABI s32 sceGameCustomDataDialogInitialize(const void* param);
PS4_SYSV_ABI s32 sceGameCustomDataDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceGameCustomDataDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceGameCustomDataDialogTerminate();
PS4_SYSV_ABI s32 sceGameCustomDataDialogUpdateStatus();

// libSceNpCommerceDialog
PS4_SYSV_ABI s32 sceNpCommerceDialogInitialize(const void* param);
PS4_SYSV_ABI s32 sceNpCommerceDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceNpCommerceDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceNpCommerceDialogTerminate();
PS4_SYSV_ABI s32 sceNpCommerceDialogUpdateStatus();

// libSceInvitationDialog
PS4_SYSV_ABI s32 sceInvitationDialogInitialize(const void* param);
PS4_SYSV_ABI s32 sceInvitationDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceInvitationDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceInvitationDialogTerminate();
PS4_SYSV_ABI s32 sceInvitationDialogUpdateStatus();

// libSceNpSnsFacebookDialog
PS4_SYSV_ABI s32 sceNpSnsFacebookDialogInitialize(const void* param);
PS4_SYSV_ABI s32 sceNpSnsFacebookDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceNpSnsFacebookDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceNpSnsFacebookDialogTerminate();
PS4_SYSV_ABI s32 sceNpSnsFacebookDialogUpdateStatus();

// libSceShareUtility
PS4_SYSV_ABI s32 sceShareUtilityOpenShareMenu(const void* param);
PS4_SYSV_ABI s32 sceShareUtilityOpenShareMenuDefault(const void* param);
PS4_SYSV_ABI s32 sceShareUtilityTerminate();

} // extern "C"

} // namespace Dialog
