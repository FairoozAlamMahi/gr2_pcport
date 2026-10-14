#include "kernel/save_data.h"
#include "common/logger.h"
#include <filesystem>
#include <cstring>
#include <string>

namespace fs = std::filesystem;

namespace SaveData {

static constexpr const char* SAVE_DIR = "./savedata";
static constexpr s32 SCE_OK = 0;
static constexpr s32 SCE_SAVE_DATA_DIALOG_STATUS_NONE = 0;
static constexpr s32 SCE_SAVE_DATA_DIALOG_STATUS_RUNNING = 1;
static constexpr s32 SCE_SAVE_DATA_DIALOG_STATUS_FINISHED = 2;

static s32 s_dialog_status = SCE_SAVE_DATA_DIALOG_STATUS_NONE;

void Initialize() {
    std::error_code ec;
    fs::create_directories(SAVE_DIR, ec);
    LOG_INFO("SAVE", "SaveData manager initialized. Root directory: " + std::string(SAVE_DIR));
}

extern "C" {

PS4_SYSV_ABI s32 sceSaveDataInitialize3(const void* initParam) {
    (void)initParam;
    Initialize();
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataTerminate() {
    LOG_INFO("SAVE", "sceSaveDataTerminate called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDirNameSearch(const void* cond, void* result) {
    (void)cond;
    u32 count = 0;
    std::error_code ec;
    if (fs::exists(SAVE_DIR, ec)) {
        for (const auto& entry : fs::directory_iterator(SAVE_DIR, ec)) {
            if (entry.is_directory()) {
                count++;
            }
        }
    }

    if (result) {
        auto* res = reinterpret_cast<u32*>(result);
        res[0] = count; // hitNum
        res[1] = count; // totalNum
    }
    LOG_INFO("SAVE", "sceSaveDataDirNameSearch found " + std::to_string(count) + " save directories");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataMount2(const void* mountParam, void* mountResult) {
    (void)mountParam;
    std::error_code ec;
    fs::create_directories(SAVE_DIR, ec);

    if (mountResult) {
        std::memset(mountResult, 0, 32);
        std::strncpy(reinterpret_cast<char*>(mountResult), "/savedata0", 15);
    }
    LOG_INFO("SAVE", "sceSaveDataMount2 mounted to /savedata0 (host: " + std::string(SAVE_DIR) + ")");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataUmount(const void* mountPoint) {
    (void)mountPoint;
    LOG_INFO("SAVE", "sceSaveDataUmount called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataSetParam(const void* mountPoint, u32 paramType, const void* paramBuf, usize bufSize) {
    (void)mountPoint;
    (void)paramType;
    (void)paramBuf;
    (void)bufSize;
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataSaveIcon(const void* mountPoint, const void* iconParam) {
    (void)mountPoint;
    (void)iconParam;
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDelete(const void* deleteParam) {
    (void)deleteParam;
    LOG_INFO("SAVE", "sceSaveDataDelete called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataTransferringMount(const void* param, void* result) {
    (void)param;
    std::error_code ec;
    fs::create_directories(SAVE_DIR, ec);
    if (result) {
        std::memset(result, 0, 32);
        std::strncpy(reinterpret_cast<char*>(result), "/savedata0", 15);
    }
    LOG_INFO("SAVE", "sceSaveDataTransferringMount mounted to /savedata0");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogInitialize() {
    s_dialog_status = SCE_SAVE_DATA_DIALOG_STATUS_NONE;
    LOG_INFO("SAVE", "sceSaveDataDialogInitialize called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogOpen(const void* param) {
    (void)param;
    s_dialog_status = SCE_SAVE_DATA_DIALOG_STATUS_FINISHED;
    LOG_INFO("SAVE", "sceSaveDataDialogOpen called -> status FINISHED");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogGetStatus() {
    return s_dialog_status == SCE_SAVE_DATA_DIALOG_STATUS_NONE ? 
           SCE_SAVE_DATA_DIALOG_STATUS_FINISHED : s_dialog_status;
}

PS4_SYSV_ABI s32 sceSaveDataDialogGetResult(void* result) {
    if (result) {
        auto* res = reinterpret_cast<s32*>(result);
        res[0] = SCE_OK; // result = 0 (SCE_OK)
        res[1] = 1;      // buttonId = 1 (OK)
    }
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogClose() {
    s_dialog_status = SCE_SAVE_DATA_DIALOG_STATUS_NONE;
    LOG_INFO("SAVE", "sceSaveDataDialogClose called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogTerminate() {
    s_dialog_status = SCE_SAVE_DATA_DIALOG_STATUS_NONE;
    LOG_INFO("SAVE", "sceSaveDataDialogTerminate called");
    return SCE_OK;
}

PS4_SYSV_ABI s32 sceSaveDataDialogUpdateStatus() {
    return SCE_SAVE_DATA_DIALOG_STATUS_FINISHED;
}

PS4_SYSV_ABI s32 sceSaveDataDialogProgressBarSetValue(u32 val) {
    (void)val;
    return SCE_OK;
}

} // extern "C"

} // namespace SaveData
