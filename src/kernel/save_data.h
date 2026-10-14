#pragma once

#include "common/types.h"

namespace SaveData {

void Initialize();

extern "C" {

// libSceSaveData
PS4_SYSV_ABI s32 sceSaveDataInitialize3(const void* initParam);
PS4_SYSV_ABI s32 sceSaveDataTerminate();
PS4_SYSV_ABI s32 sceSaveDataDirNameSearch(const void* cond, void* result);
PS4_SYSV_ABI s32 sceSaveDataMount2(const void* mountParam, void* mountResult);
PS4_SYSV_ABI s32 sceSaveDataUmount(const void* mountPoint);
PS4_SYSV_ABI s32 sceSaveDataSetParam(const void* mountPoint, u32 paramType, const void* paramBuf, usize bufSize);
PS4_SYSV_ABI s32 sceSaveDataSaveIcon(const void* mountPoint, const void* iconParam);
PS4_SYSV_ABI s32 sceSaveDataDelete(const void* deleteParam);
PS4_SYSV_ABI s32 sceSaveDataTransferringMount(const void* param, void* result);

// libSceSaveDataDialog
PS4_SYSV_ABI s32 sceSaveDataDialogInitialize();
PS4_SYSV_ABI s32 sceSaveDataDialogOpen(const void* param);
PS4_SYSV_ABI s32 sceSaveDataDialogGetStatus();
PS4_SYSV_ABI s32 sceSaveDataDialogGetResult(void* result);
PS4_SYSV_ABI s32 sceSaveDataDialogClose();
PS4_SYSV_ABI s32 sceSaveDataDialogTerminate();
PS4_SYSV_ABI s32 sceSaveDataDialogUpdateStatus();
PS4_SYSV_ABI s32 sceSaveDataDialogProgressBarSetValue(u32 val);

} // extern "C"

} // namespace SaveData
