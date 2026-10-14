#include "kernel/content_service.h"
#include "common/logger.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstring>
#include <string>

namespace fs = std::filesystem;

namespace Content {

static constexpr const char* PHOTO_DIR = "./savedata/photos";

void Initialize() {
    std::error_code ec;
    fs::create_directories(PHOTO_DIR, ec);
    LOG_INFO("CONTENT", "ContentSearch & ContentExport services initialized");
}

void Shutdown() {
    LOG_INFO("CONTENT", "ContentSearch & ContentExport services shutdown");
}

extern "C" {

// ============================================================================
// libSceContentSearch
// ============================================================================

PS4_SYSV_ABI s32 sceContentSearchInit(const void* param) {
    (void)param;
    LOG_INFO("CONTENT", "sceContentSearchInit: DLC search initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchTerm() {
    LOG_INFO("CONTENT", "sceContentSearchTerm: DLC search terminated");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchGetNumOfContent(u32* num_content) {
    if (num_content) {
        *num_content = 0; // DLC count
    }
    LOG_INFO("CONTENT", "sceContentSearchGetNumOfContent -> 0");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchSearchContent(const void* search_param, void* result) {
    (void)search_param;
    (void)result;
    LOG_INFO("CONTENT", "sceContentSearchSearchContent called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchOpenMetadataByContentId(const void* content_id, void* meta_handle) {
    (void)content_id;
    if (meta_handle) {
        *reinterpret_cast<u64*>(meta_handle) = 0x1001;
    }
    LOG_INFO("CONTENT", "sceContentSearchOpenMetadataByContentId called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchCloseMetadata(void* meta_handle) {
    (void)meta_handle;
    LOG_INFO("CONTENT", "sceContentSearchCloseMetadata called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchGetMetadataValue(void* meta_handle, u32 key, void* val, usize* val_size) {
    (void)meta_handle;
    (void)key;
    (void)val;
    if (val_size) {
        *val_size = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceContentSearchGetMyApplicationIndex(u32* app_index) {
    if (app_index) {
        *app_index = 0;
    }
    LOG_INFO("CONTENT", "sceContentSearchGetMyApplicationIndex -> 0");
    return 0;
}

// ============================================================================
// libSceContentExport
// ============================================================================

PS4_SYSV_ABI s32 sceContentExportInit(const void* param) {
    (void)param;
    std::error_code ec;
    fs::create_directories(PHOTO_DIR, ec);
    LOG_INFO("CONTENT", "sceContentExportInit: photo export subsystem initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceContentExportTerm() {
    LOG_INFO("CONTENT", "sceContentExportTerm: photo export subsystem terminated");
    return 0;
}

PS4_SYSV_ABI s32 sceContentExportStart(const void* param, void* handle) {
    (void)param;
    if (handle) {
        *reinterpret_cast<u64*>(handle) = 0x2001;
    }
    LOG_INFO("CONTENT", "sceContentExportStart called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentExportFinish(void* handle) {
    (void)handle;
    LOG_INFO("CONTENT", "sceContentExportFinish called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentExportFromData(const void* data, usize size, const void* param, void* out_id) {
    (void)param;
    if (out_id) {
        *reinterpret_cast<u32*>(out_id) = 1;
    }
    if (data && size > 0) {
        std::error_code ec;
        fs::create_directories(PHOTO_DIR, ec);
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string filename = std::string(PHOTO_DIR) + "/photo_" + std::to_string(ms) + ".jpg";
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(data), size);
            out.close();
            LOG_INFO("CONTENT", "sceContentExportFromData: exported photo to " + filename + " (" + std::to_string(size) + " bytes)");
        } else {
            LOG_WARN("CONTENT", "sceContentExportFromData: failed to open " + filename + " for writing");
        }
    }
    return 0;
}

PS4_SYSV_ABI s32 sceContentExportFromDataWithThumbnail(const void* data, usize size, const void* thumb_data, usize thumb_size, const void* param, void* out_id) {
    (void)thumb_data;
    (void)thumb_size;
    return sceContentExportFromData(data, size, param, out_id);
}

// ============================================================================
// libSceContentDelete
// ============================================================================

PS4_SYSV_ABI s32 sceContentDeleteInitialize(const void* param) {
    (void)param;
    LOG_INFO("CONTENT", "sceContentDeleteInitialize called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentDeleteTerminate() {
    LOG_INFO("CONTENT", "sceContentDeleteTerminate called");
    return 0;
}

PS4_SYSV_ABI s32 sceContentDeleteById(const void* id) {
    (void)id;
    LOG_INFO("CONTENT", "sceContentDeleteById called");
    return 0;
}

} // extern "C"

} // namespace Content
