#include "video/font_service.h"
#include "common/logger.h"
#include <cstring>
#include <atomic>

namespace Font {

static std::atomic<u32> s_next_handle{1};

extern "C" {

PS4_SYSV_ABI s32 sceFontMemoryInit(void* font_memory, void* address, u32 size_byte, void* mem_interface, void* mspace_obj, void* destroy_cb, void* destroy_obj) {
    (void)address;
    (void)mem_interface;
    (void)mspace_obj;
    (void)destroy_cb;
    (void)destroy_obj;
    if (font_memory) {
        std::memset(font_memory, 0, 64);
    }
    LOG_INFO("FONT", "sceFontMemoryInit: initialized font allocator (size=" + std::to_string(size_byte) + ")");
    return 0;
}

PS4_SYSV_ABI s32 sceFontMemoryTerm(void* font_memory) {
    if (font_memory) {
        std::memset(font_memory, 0, 64);
    }
    LOG_INFO("FONT", "sceFontMemoryTerm called");
    return 0;
}

static inline bool IsValidUserPointer(void* p) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    return (addr >= 0x10000 && addr < 0x0000800000000000ULL);
}

PS4_SYSV_ABI s32 sceFontSelectLibraryFt(void* arg0, void* arg1, void* arg2, void* arg3) {
    (void)arg2;
    (void)arg3;
    if (IsValidUserPointer(arg0)) {
        *reinterpret_cast<uintptr_t*>(arg0) = 1;
    }
    if (IsValidUserPointer(arg1)) {
        *reinterpret_cast<uintptr_t*>(arg1) = 1;
    }
    LOG_INFO("FONT", "sceFontSelectLibraryFt: FreeType library backend selected");
    return 0;
}

PS4_SYSV_ABI s32 sceFontCreateLibraryWithEdition(void* memory, void* selection, u64 edition, void** pLibrary) {
    (void)memory;
    (void)selection;
    (void)edition;
    u32 handle = s_next_handle.fetch_add(1);
    if (IsValidUserPointer(pLibrary)) {
        *pLibrary = reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
    }
    LOG_INFO("FONT", "sceFontCreateLibraryWithEdition: created font library handle " + std::to_string(handle));
    return 0;
}

PS4_SYSV_ABI s32 sceFontDestroyLibrary(void** pLibrary) {
    if (IsValidUserPointer(pLibrary)) {
        *pLibrary = nullptr;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontSupportExternalFonts(void* library, u32 font_max, u32 formats) {
    (void)library;
    LOG_INFO("FONT", "sceFontSupportExternalFonts: max=" + std::to_string(font_max) + ", formats=0x" + std::to_string(formats));
    return 0;
}

PS4_SYSV_ABI s32 sceFontSelectRendererFt(void* arg0, void* arg1, void* arg2, void* arg3) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    if (IsValidUserPointer(arg0)) {
        *reinterpret_cast<uintptr_t*>(arg0) = 1;
    }
    if (IsValidUserPointer(arg1)) {
        *reinterpret_cast<uintptr_t*>(arg1) = 1;
    }
    if (IsValidUserPointer(arg3)) {
        *reinterpret_cast<uintptr_t*>(arg3) = 1;
    }
    LOG_INFO("FONT", "sceFontSelectRendererFt: FreeType renderer backend selected");
    return 0;
}

PS4_SYSV_ABI s32 sceFontCreateRendererWithEdition(void* memory, void* selection, u64 edition, void** pRenderer) {
    (void)memory;
    (void)selection;
    (void)edition;
    u32 handle = s_next_handle.fetch_add(1);
    if (IsValidUserPointer(pRenderer)) {
        *pRenderer = reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
    }
    LOG_INFO("FONT", "sceFontCreateRendererWithEdition: created renderer handle " + std::to_string(handle));
    return 0;
}

PS4_SYSV_ABI s32 sceFontDestroyRenderer(void** pRenderer) {
    if (IsValidUserPointer(pRenderer)) {
        *pRenderer = nullptr;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontGetPixelResolution(void* arg0, void* arg1, void* arg2) {
    if (IsValidUserPointer(arg0)) {
        float* f0 = static_cast<float*>(arg0);
        f0[0] = 72.0f;
        f0[1] = 72.0f;
    }
    if (IsValidUserPointer(arg1)) {
        float* f1 = static_cast<float*>(arg1);
        f1[0] = 72.0f;
    }
    if (IsValidUserPointer(arg2)) {
        float* f2 = static_cast<float*>(arg2);
        f2[0] = 72.0f;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontOpenFontMemory(void* library, void* font_address, u32 font_size, void* detail, void** pFontHandle) {
    (void)library;
    (void)font_address;
    u32 handle = s_next_handle.fetch_add(1);
    if (IsValidUserPointer(pFontHandle)) {
        *pFontHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
    } else if (IsValidUserPointer(detail)) {
        *reinterpret_cast<uintptr_t*>(detail) = handle;
    }
    LOG_INFO("FONT", "sceFontOpenFontMemory: opened font handle " + std::to_string(handle) + 
                     " (size=" + std::to_string(font_size) + ")");
    return 0;
}

PS4_SYSV_ABI s32 sceFontCloseFont(void* font_handle) {
    (void)font_handle;
    LOG_INFO("FONT", "sceFontCloseFont called");
    return 0;
}

PS4_SYSV_ABI s32 sceFontBindRenderer(void* font_handle, void* renderer) {
    (void)font_handle;
    (void)renderer;
    LOG_INFO("FONT", "sceFontBindRenderer: bound font handle to renderer");
    return 0;
}

PS4_SYSV_ABI s32 sceFontUnbindRenderer(void* font_handle) {
    (void)font_handle;
    return 0;
}

PS4_SYSV_ABI s32 sceFontSetScalePixel(void* font_handle, float w, float h) {
    (void)font_handle;
    (void)w;
    (void)h;
    return 0;
}

PS4_SYSV_ABI s32 sceFontSetupRenderScalePixel(void* font_handle, float w, float h) {
    (void)font_handle;
    (void)w;
    (void)h;
    return 0;
}

PS4_SYSV_ABI s32 sceFontGetHorizontalLayout(void* arg0, void* arg1) {
    void* target = arg1 ? arg1 : arg0;
    if (IsValidUserPointer(target)) {
        float* f = static_cast<float*>(target);
        f[0] = 18.0f; // BaseLineY
        f[1] = 24.0f; // LineHeight
        f[2] = 24.0f; // EffectHeight
        f[3] = 0.0f;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontGetCharGlyphMetrics(void* font_handle, u32 code, void* metrics) {
    (void)font_handle;
    (void)code;
    if (IsValidUserPointer(metrics)) {
        float* f = static_cast<float*>(metrics);
        f[0] = 16.0f; // width
        f[1] = 16.0f; // height
        f[2] = 0.0f;  // bearing_x
        f[3] = 14.0f; // bearing_y
        f[4] = 16.0f; // advance_x
        f[5] = 0.0f;  // advance_y
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontGetRenderScaledKerning(void* font_handle, u32 pre_code, u32 code, void* kerning) {
    (void)font_handle;
    (void)pre_code;
    (void)code;
    if (IsValidUserPointer(kerning)) {
        float* f = static_cast<float*>(kerning);
        f[0] = 0.0f; // offset_x
        f[1] = 0.0f; // offset_y
        f[2] = 0.0f; // position_x
    }
    return 0;
}

PS4_SYSV_ABI s32 sceFontRenderSurfaceInit(void* surf, void* buffer, s32 buf_width_byte, s32 pixel_size_byte, s32 w, s32 h) {
    (void)surf;
    (void)buffer;
    (void)buf_width_byte;
    (void)pixel_size_byte;
    (void)w;
    (void)h;
    return 0;
}

PS4_SYSV_ABI s32 sceFontRenderCharGlyphImageHorizontal(void* font_handle, u32 code, void* surf, float x, float y, void* metrics, void* result) {
    (void)font_handle;
    (void)code;
    (void)surf;
    (void)x;
    (void)y;
    if (IsValidUserPointer(metrics)) {
        float* f = static_cast<float*>(metrics);
        f[0] = 16.0f;
        f[1] = 16.0f;
        f[2] = 0.0f;
        f[3] = 14.0f;
        f[4] = 16.0f;
        f[5] = 0.0f;
    }
    if (IsValidUserPointer(result)) {
        *reinterpret_cast<u32*>(result) = 0;
    }
    return 0;
}

} // extern "C"

} // namespace Font
