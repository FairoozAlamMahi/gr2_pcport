#pragma once

#include "common/types.h"

namespace Font {

struct HorizontalLayout {
    float baseline_y{18.0f};
    float line_height{24.0f};
    float effect_height{24.0f};
    float reserved{0.0f};
};

struct GlyphMetrics {
    float width{16.0f};
    float height{16.0f};
    float bearing_x{0.0f};
    float bearing_y{14.0f};
    float advance_x{16.0f};
    float advance_y{0.0f};
};

extern "C" {

PS4_SYSV_ABI s32 sceFontMemoryInit(void* font_memory, void* address, u32 size_byte, void* mem_interface, void* mspace_obj, void* destroy_cb, void* destroy_obj);
PS4_SYSV_ABI s32 sceFontMemoryTerm(void* font_memory);

PS4_SYSV_ABI s32 sceFontSelectLibraryFt(void* arg0, void* arg1, void* arg2, void* arg3);
PS4_SYSV_ABI s32 sceFontCreateLibraryWithEdition(void* memory, void* selection, u64 edition, void** pLibrary);
PS4_SYSV_ABI s32 sceFontDestroyLibrary(void** pLibrary);

PS4_SYSV_ABI s32 sceFontSupportExternalFonts(void* library, u32 font_max, u32 formats);

PS4_SYSV_ABI s32 sceFontSelectRendererFt(void* arg0, void* arg1, void* arg2, void* arg3);
PS4_SYSV_ABI s32 sceFontCreateRendererWithEdition(void* memory, void* selection, u64 edition, void** pRenderer);
PS4_SYSV_ABI s32 sceFontDestroyRenderer(void** pRenderer);

PS4_SYSV_ABI s32 sceFontGetPixelResolution(void* arg0, void* arg1, void* arg2);

PS4_SYSV_ABI s32 sceFontOpenFontMemory(void* library, void* font_address, u32 font_size, void* detail, void** pFontHandle);
PS4_SYSV_ABI s32 sceFontCloseFont(void* font_handle);

PS4_SYSV_ABI s32 sceFontBindRenderer(void* font_handle, void* renderer);
PS4_SYSV_ABI s32 sceFontUnbindRenderer(void* font_handle);

PS4_SYSV_ABI s32 sceFontSetScalePixel(void* font_handle, float w, float h);
PS4_SYSV_ABI s32 sceFontSetupRenderScalePixel(void* font_handle, float w, float h);

PS4_SYSV_ABI s32 sceFontGetHorizontalLayout(void* arg0, void* arg1);

PS4_SYSV_ABI s32 sceFontGetCharGlyphMetrics(void* font_handle, u32 code, void* metrics);
PS4_SYSV_ABI s32 sceFontGetRenderScaledKerning(void* font_handle, u32 pre_code, u32 code, void* kerning);
PS4_SYSV_ABI s32 sceFontRenderSurfaceInit(void* surf, void* buffer, s32 buf_width_byte, s32 pixel_size_byte, s32 w, s32 h);
PS4_SYSV_ABI s32 sceFontRenderCharGlyphImageHorizontal(void* font_handle, u32 code, void* surf, float x, float y, void* metrics, void* result);

} // extern "C"

} // namespace Font
