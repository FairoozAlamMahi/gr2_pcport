#include "kernel/security_service.h"
#include "common/logger.h"
#include <cstring>
#include <string>

namespace Security {

extern "C" {

PS4_SYSV_ABI s32 sceLibSecureInit(const void* param) {
    (void)param;
    LOG_INFO("SECURE", "sceLibSecureInit initialized security and cryptography subsystem");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureDestroy() {
    LOG_INFO("SECURE", "sceLibSecureDestroy called");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyGetContextSize(u32 cipher, u32 mode, usize* size) {
    (void)cipher;
    (void)mode;
    if (size) {
        *size = 128; // 128 bytes context buffer
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographyGetContextSize returned 128 bytes");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographySetContext(void* ctx, u32 cipher, u32 mode, const void* key, usize key_len, const void* iv, usize iv_len) {
    (void)cipher;
    (void)mode;
    (void)key;
    (void)key_len;
    (void)iv;
    (void)iv_len;
    if (ctx) {
        std::memset(ctx, 0, 128);
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographySetContext configured crypto context (cipher=" +
                       std::to_string(cipher) + ", mode=" + std::to_string(mode) + ")");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyEncrypt(void* ctx, const void* src, usize src_len, void* dst, usize* dst_len) {
    (void)ctx;
    if (dst && src && src_len > 0) {
        std::memcpy(dst, src, src_len);
    }
    if (dst_len) {
        *dst_len = src_len;
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographyEncrypt encrypted " + std::to_string(src_len) + " bytes (passthrough)");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyDecrypt(void* ctx, const void* src, usize src_len, void* dst, usize* dst_len) {
    (void)ctx;
    if (dst && src && src_len > 0) {
        std::memcpy(dst, src, src_len);
    }
    if (dst_len) {
        *dst_len = src_len;
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographyDecrypt decrypted " + std::to_string(src_len) + " bytes (passthrough)");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyDeleteContext(void* ctx) {
    if (ctx) {
        std::memset(ctx, 0, 128);
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographyDeleteContext called");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyMessagePadding(void* ctx, const void* in_buf, usize in_len, void* out_buf, usize* out_len) {
    (void)ctx;
    if (out_buf && in_buf && in_len > 0) {
        std::memcpy(out_buf, in_buf, in_len);
    }
    if (out_len) {
        *out_len = in_len;
    }
    LOG_INFO("SECURE", "sceLibSecureCryptographyMessagePadding applied message padding (" + std::to_string(in_len) + " bytes)");
    return 0;
}

PS4_SYSV_ABI s32 sceLibSecureCryptographyMessagePaddingSize(u32 cipher, u32 mode, usize in_len, usize* out_len) {
    (void)cipher;
    (void)mode;
    if (out_len) {
        *out_len = in_len;
    }
    return 0;
}

} // extern "C"

} // namespace Security
