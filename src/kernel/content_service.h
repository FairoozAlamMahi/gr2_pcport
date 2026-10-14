#pragma once

#include "common/types.h"

namespace Content {

void Initialize();
void Shutdown();

extern "C" {

// ============================================================================
// libSceContentSearch
// ============================================================================
PS4_SYSV_ABI s32 sceContentSearchInit(const void* param);
PS4_SYSV_ABI s32 sceContentSearchTerm();
PS4_SYSV_ABI s32 sceContentSearchGetNumOfContent(u32* num_content);
PS4_SYSV_ABI s32 sceContentSearchSearchContent(const void* search_param, void* result);
PS4_SYSV_ABI s32 sceContentSearchOpenMetadataByContentId(const void* content_id, void* meta_handle);
PS4_SYSV_ABI s32 sceContentSearchCloseMetadata(void* meta_handle);
PS4_SYSV_ABI s32 sceContentSearchGetMetadataValue(void* meta_handle, u32 key, void* val, usize* val_size);
PS4_SYSV_ABI s32 sceContentSearchGetMyApplicationIndex(u32* app_index);

// ============================================================================
// libSceContentExport
// ============================================================================
PS4_SYSV_ABI s32 sceContentExportInit(const void* param);
PS4_SYSV_ABI s32 sceContentExportTerm();
PS4_SYSV_ABI s32 sceContentExportStart(const void* param, void* handle);
PS4_SYSV_ABI s32 sceContentExportFinish(void* handle);
PS4_SYSV_ABI s32 sceContentExportFromData(const void* data, usize size, const void* param, void* out_id);
PS4_SYSV_ABI s32 sceContentExportFromDataWithThumbnail(const void* data, usize size, const void* thumb_data, usize thumb_size, const void* param, void* out_id);

// ============================================================================
// libSceContentDelete
// ============================================================================
PS4_SYSV_ABI s32 sceContentDeleteInitialize(const void* param);
PS4_SYSV_ABI s32 sceContentDeleteTerminate();
PS4_SYSV_ABI s32 sceContentDeleteById(const void* id);

} // extern "C"

} // namespace Content
