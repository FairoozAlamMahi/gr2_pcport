#include "video/image_decoder.h"
#include "common/logger.h"
#include <windows.h>
#include <cstring>
#include <string>

namespace ImageDecoder {

struct PngDecoderContext {
    u32 magic{0x504E4744}; // "PNGD"
    u32 width{1920};
    u32 height{1080};
};

struct JpegDecoderContext {
    u32 magic{0x4A504547}; // "JPEG"
    u32 width{1920};
    u32 height{1080};
};

static PngDecoderContext s_dummy_png_context;
static JpegDecoderContext s_dummy_jpeg_context;

static bool IsReadableBuffer(const void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    if (addr < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) {
        return false;
    }
    uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return (addr + size) <= region_end;
}

static bool TryParsePngIhdr(const void* parseParam, u32& out_w, u32& out_h) {
    if (!parseParam) return false;

    const u8* stream = nullptr;
    static const u8 png_signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

    // Check if parseParam directly points to the PNG stream
    if (IsReadableBuffer(parseParam, 24) && std::memcmp(parseParam, png_signature, 8) == 0) {
        stream = static_cast<const u8*>(parseParam);
    }
    // Check if parseParam is a struct whose first member is a pointer to the PNG stream
    else if (IsReadableBuffer(parseParam, sizeof(void*))) {
        const u8* indirect = *reinterpret_cast<const u8* const*>(parseParam);
        if (indirect && IsReadableBuffer(indirect, 24) && std::memcmp(indirect, png_signature, 8) == 0) {
            stream = indirect;
        }
    }

    if (!stream) return false;

    // Check IHDR chunk header (at offset 12: 'I', 'H', 'D', 'R')
    if (stream[12] == 'I' && stream[13] == 'H' && stream[14] == 'D' && stream[15] == 'R') {
        u32 w = (static_cast<u32>(stream[16]) << 24) |
                (static_cast<u32>(stream[17]) << 16) |
                (static_cast<u32>(stream[18]) << 8)  |
                static_cast<u32>(stream[19]);
        u32 h = (static_cast<u32>(stream[20]) << 24) |
                (static_cast<u32>(stream[21]) << 16) |
                (static_cast<u32>(stream[22]) << 8)  |
                static_cast<u32>(stream[23]);

        if (w > 0 && h > 0 && w <= 16384 && h <= 16384) {
            out_w = w;
            out_h = h;
            return true;
        }
    }

    return false;
}

static bool TryParseJpegSof(const void* parseParam, u32& out_w, u32& out_h) {
    if (!parseParam) return false;

    const u8* stream = nullptr;

    // Check if parseParam directly points to the JPEG stream (starts with SOI: 0xFF, 0xD8)
    if (IsReadableBuffer(parseParam, 4) &&
        static_cast<const u8*>(parseParam)[0] == 0xFF &&
        static_cast<const u8*>(parseParam)[1] == 0xD8) {
        stream = static_cast<const u8*>(parseParam);
    }
    // Check if parseParam is a struct whose first member is a pointer to the JPEG stream
    else if (IsReadableBuffer(parseParam, sizeof(void*))) {
        const u8* indirect = *reinterpret_cast<const u8* const*>(parseParam);
        if (indirect && IsReadableBuffer(indirect, 4) &&
            indirect[0] == 0xFF && indirect[1] == 0xD8) {
            stream = indirect;
        }
    }

    if (!stream) return false;

    size_t offset = 2;
    constexpr size_t MAX_SCAN = 65536;

    while (offset + 4 < MAX_SCAN && IsReadableBuffer(stream + offset, 8)) {
        if (stream[offset] != 0xFF) {
            offset++;
            continue;
        }
        while (offset < MAX_SCAN && stream[offset] == 0xFF) {
            offset++;
        }
        if (offset >= MAX_SCAN) break;

        u8 marker = stream[offset++];
        if (marker == 0xD9 || marker == 0xDA) { // EOI or SOS marker
            break;
        }

        if (offset + 2 > MAX_SCAN || !IsReadableBuffer(stream + offset, 2)) break;
        u16 len = (static_cast<u16>(stream[offset]) << 8) | stream[offset + 1];
        if (len < 2) break;

        // SOF0 (Baseline DCT), SOF1 (Extended Sequential), SOF2 (Progressive)
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            if (offset + 7 < MAX_SCAN && IsReadableBuffer(stream + offset, 7)) {
                u32 h = (static_cast<u32>(stream[offset + 3]) << 8) | stream[offset + 4];
                u32 w = (static_cast<u32>(stream[offset + 5]) << 8) | stream[offset + 6];
                if (w > 0 && h > 0) {
                    out_w = w;
                    out_h = h;
                    return true;
                }
            }
        }
        offset += len;
    }

    return false;
}

extern "C" {

// ========================================================================
// libScePngDec
// ========================================================================

PS4_SYSV_ABI s32 scePngDecCreate(const void* initParam, void** handle) {
    (void)initParam;
    if (handle) {
        *handle = &s_dummy_png_context;
    }
    LOG_INFO("PNGDEC", "scePngDecCreate initialized decoder handle");
    return 0;
}

PS4_SYSV_ABI s32 scePngDecDelete(void* handle) {
    (void)handle;
    LOG_INFO("PNGDEC", "scePngDecDelete called");
    return 0;
}

PS4_SYSV_ABI s32 scePngDecParseHeader(void* handle, const void* parseParam, void* header) {
    (void)handle;
    u32 width = 1920;
    u32 height = 1080;

    if (TryParsePngIhdr(parseParam, width, height)) {
        LOG_INFO("PNGDEC", "scePngDecParseHeader parsed IHDR dimensions: " +
                           std::to_string(width) + "x" + std::to_string(height));
    } else {
        LOG_INFO("PNGDEC", "scePngDecParseHeader defaulting dimensions to 1920x1080");
    }

    if (header) {
        u32* hdr = reinterpret_cast<u32*>(header);
        hdr[0] = width;
        hdr[1] = height;
    }
    return 0;
}

PS4_SYSV_ABI s32 scePngDecDecode(void* handle, const void* decodeParam, void* image) {
    (void)handle;
    (void)decodeParam;
    (void)image;
    LOG_INFO("PNGDEC", "scePngDecDecode decoded image successfully");
    return 0;
}

PS4_SYSV_ABI s32 scePngDecQueryMemorySize(const void* parseParam, usize* size) {
    (void)parseParam;
    if (size) {
        *size = 0x800000; // 8 MB
    }
    LOG_INFO("PNGDEC", "scePngDecQueryMemorySize returned 8 MB");
    return 0;
}

// ========================================================================
// libSceJpegDec
// ========================================================================

PS4_SYSV_ABI s32 sceJpegDecCreate(const void* initParam, void** handle) {
    (void)initParam;
    if (handle) {
        *handle = &s_dummy_jpeg_context;
    }
    LOG_INFO("JPEGDEC", "sceJpegDecCreate initialized decoder handle");
    return 0;
}

PS4_SYSV_ABI s32 sceJpegDecDelete(void* handle) {
    (void)handle;
    LOG_INFO("JPEGDEC", "sceJpegDecDelete called");
    return 0;
}

PS4_SYSV_ABI s32 sceJpegDecParseHeader(void* handle, const void* parseParam, void* header) {
    (void)handle;
    u32 width = 1920;
    u32 height = 1080;

    if (TryParseJpegSof(parseParam, width, height)) {
        LOG_INFO("JPEGDEC", "sceJpegDecParseHeader parsed SOF marker dimensions: " +
                           std::to_string(width) + "x" + std::to_string(height));
    } else {
        LOG_INFO("JPEGDEC", "sceJpegDecParseHeader defaulting dimensions to 1920x1080");
    }

    if (header) {
        u32* hdr = reinterpret_cast<u32*>(header);
        hdr[0] = width;
        hdr[1] = height;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceJpegDecDecode(void* handle, const void* decodeParam, void* image) {
    (void)handle;
    (void)decodeParam;
    (void)image;
    LOG_INFO("JPEGDEC", "sceJpegDecDecode decoded image successfully");
    return 0;
}

PS4_SYSV_ABI s32 sceJpegDecQueryMemorySize(const void* parseParam, usize* size) {
    (void)parseParam;
    if (size) {
        *size = 0x800000; // 8 MB
    }
    LOG_INFO("JPEGDEC", "sceJpegDecQueryMemorySize returned 8 MB");
    return 0;
}

} // extern "C"

} // namespace ImageDecoder
