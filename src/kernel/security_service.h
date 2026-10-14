#pragma once

#include "common/types.h"

namespace Security {

extern "C" {

PS4_SYSV_ABI s32 sceLibSecureInit(const void* param = nullptr);
PS4_SYSV_ABI s32 sceLibSecureDestroy();

PS4_SYSV_ABI s32 sceLibSecureCryptographyGetContextSize(u32 cipher, u32 mode, usize* size);
PS4_SYSV_ABI s32 sceLibSecureCryptographySetContext(void* ctx, u32 cipher, u32 mode, const void* key, usize key_len, const void* iv, usize iv_len);
PS4_SYSV_ABI s32 sceLibSecureCryptographyEncrypt(void* ctx, const void* src, usize src_len, void* dst, usize* dst_len);
PS4_SYSV_ABI s32 sceLibSecureCryptographyDecrypt(void* ctx, const void* src, usize src_len, void* dst, usize* dst_len);
PS4_SYSV_ABI s32 sceLibSecureCryptographyDeleteContext(void* ctx);
PS4_SYSV_ABI s32 sceLibSecureCryptographyMessagePadding(void* ctx = nullptr, const void* in_buf = nullptr, usize in_len = 0, void* out_buf = nullptr, usize* out_len = nullptr);
PS4_SYSV_ABI s32 sceLibSecureCryptographyMessagePaddingSize(u32 cipher, u32 mode, usize in_len, usize* out_len);

} // extern "C"

} // namespace Security
