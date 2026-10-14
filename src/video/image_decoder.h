#pragma once

#include "common/types.h"

namespace ImageDecoder {

extern "C" {

// libScePngDec
PS4_SYSV_ABI s32 scePngDecCreate(const void* initParam, void** handle);
PS4_SYSV_ABI s32 scePngDecDelete(void* handle);
PS4_SYSV_ABI s32 scePngDecParseHeader(void* handle, const void* parseParam, void* header);
PS4_SYSV_ABI s32 scePngDecDecode(void* handle, const void* decodeParam, void* image);
PS4_SYSV_ABI s32 scePngDecQueryMemorySize(const void* parseParam, usize* size);

// libSceJpegDec
PS4_SYSV_ABI s32 sceJpegDecCreate(const void* initParam, void** handle);
PS4_SYSV_ABI s32 sceJpegDecDelete(void* handle);
PS4_SYSV_ABI s32 sceJpegDecParseHeader(void* handle, const void* parseParam, void* header);
PS4_SYSV_ABI s32 sceJpegDecDecode(void* handle, const void* decodeParam, void* image);
PS4_SYSV_ABI s32 sceJpegDecQueryMemorySize(const void* parseParam, usize* size);

// libSceJpegEnc
PS4_SYSV_ABI s32 sceJpegEncCreate(const void* initParam, void** handle);
PS4_SYSV_ABI s32 sceJpegEncDelete(void* handle);
PS4_SYSV_ABI s32 sceJpegEncEncode(void* handle, const void* encodeParam, void* image);
PS4_SYSV_ABI s32 sceJpegEncQueryMemorySize(const void* encodeParam, usize* size);

} // extern "C"

} // namespace ImageDecoder
