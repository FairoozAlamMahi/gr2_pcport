#include "pad/pad_handler.h"
#include "common/logger.h"
#include <xinput.h>
#include <cmath>
#include <chrono>

namespace Pad {

enum class SwipeGesture {
    None,
    SwipeUp,   // Lunar Style
    SwipeDown, // Jupiter Style
    TapCenter  // Normal Style
};

struct SyntheticSwipeState {
    SwipeGesture gesture{SwipeGesture::None};
    int current_frame{0};
    static constexpr int TOTAL_FRAMES = 8;
};

static SyntheticSwipeState s_swipe{};
static POINT s_last_mouse_pos{};
static bool s_has_mouse_init = false;

void Initialize() {
    LOG_INFO("PAD", "Initializing Gamepad & Synthetic Touchpad Swipe Engine...");
    GetCursorPos(&s_last_mouse_pos);
    s_has_mouse_init = true;
}

void UpdateInput() {
    // Process host mouse movement for gyro fallback
    POINT cur_mouse;
    GetCursorPos(&cur_mouse);
    float dx = 0.0f;
    float dy = 0.0f;
    if (s_has_mouse_init) {
        dx = static_cast<float>(cur_mouse.x - s_last_mouse_pos.x);
        dy = static_cast<float>(cur_mouse.y - s_last_mouse_pos.y);
    }
    s_last_mouse_pos = cur_mouse;
    (void)dx;
    (void)dy;

    // Advance synthetic swipe frame if active
    if (s_swipe.gesture != SwipeGesture::None) {
        s_swipe.current_frame++;
        if (s_swipe.current_frame >= SyntheticSwipeState::TOTAL_FRAMES) {
            s_swipe.gesture = SwipeGesture::None;
            s_swipe.current_frame = 0;
        }
    }
}

extern "C" {

PS4_SYSV_ABI s32 scePadOpen(s32 user_id, s32 type, s32 index, const void* param) {
    LOG_INFO("PAD", "scePadOpen called for user_id=" + std::to_string(user_id));
    return 1; // Return handle 1
}

PS4_SYSV_ABI s32 scePadClose(s32 handle) {
    return 0;
}

PS4_SYSV_ABI s32 scePadReadState(s32 handle, ScePadData* data) {
    if (!data) return -1;
    std::memset(data, 0, sizeof(ScePadData));
    data->connected = 1;
    data->connectedCount = 1;

    // 1. Read XInput Controller
    XINPUT_STATE xi_state{};
    DWORD dwResult = XInputGetState(0, &xi_state);

    if (dwResult == ERROR_SUCCESS) {
        WORD wButtons = xi_state.Gamepad.wButtons;

        // Map PS4 buttons (standard Orbis bitmask)
        // Triangle (0x1000), Circle (0x2000), Cross (0x4000), Square (0x8000)
        // L1 (0x0400), R1 (0x0800), L2 (0x0100), R2 (0x0200)
        // D-Pad Up (0x0010), Down (0x0040), Left (0x0080), Right (0x0020)
        u32 ps4_btn = 0;
        if (wButtons & XINPUT_GAMEPAD_A) ps4_btn |= 0x4000; // Cross
        if (wButtons & XINPUT_GAMEPAD_B) ps4_btn |= 0x2000; // Circle
        if (wButtons & XINPUT_GAMEPAD_X) ps4_btn |= 0x8000; // Square
        if (wButtons & XINPUT_GAMEPAD_Y) ps4_btn |= 0x1000; // Triangle

        if (wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ps4_btn |= 0x0400; // L1
        if (wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ps4_btn |= 0x0800; // R1

        if (xi_state.Gamepad.bLeftTrigger > 30)  ps4_btn |= 0x0100; // L2
        if (xi_state.Gamepad.bRightTrigger > 30) ps4_btn |= 0x0200; // R2

        if (wButtons & XINPUT_GAMEPAD_DPAD_UP)    ps4_btn |= 0x0010;
        if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ps4_btn |= 0x0020;
        if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ps4_btn |= 0x0040;
        if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ps4_btn |= 0x0080;

        if (wButtons & XINPUT_GAMEPAD_START) ps4_btn |= 0x0008; // Options
        if (wButtons & XINPUT_GAMEPAD_BACK)  ps4_btn |= 0x0001; // Touchpad Click

        data->buttons = ps4_btn;

        // Thumbsticks: map [-32768, 32767] -> [0, 255]
        data->leftStickX  = static_cast<u8>(((xi_state.Gamepad.sThumbLX + 32768) >> 8) & 0xFF);
        data->leftStickY  = static_cast<u8>(((32767 - xi_state.Gamepad.sThumbLY) >> 8) & 0xFF);
        data->rightStickX = static_cast<u8>(((xi_state.Gamepad.sThumbRX + 32768) >> 8) & 0xFF);
        data->rightStickY = static_cast<u8>(((32767 - xi_state.Gamepad.sThumbRY) >> 8) & 0xFF);

        // 2. Synthetic Touchpad Swipe Engine for Gravity Style Switching:
        // LB + D-Pad Up   -> Lunar Style (Swipe Up)
        // LB + D-Pad Down -> Jupiter Style (Swipe Down)
        // LB + D-Pad Left -> Normal Style (Center Tap)
        if (wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) {
            if ((wButtons & XINPUT_GAMEPAD_DPAD_UP) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::SwipeUp;
                s_swipe.current_frame = 0;
            } else if ((wButtons & XINPUT_GAMEPAD_DPAD_DOWN) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::SwipeDown;
                s_swipe.current_frame = 0;
            } else if ((wButtons & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT)) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::TapCenter;
                s_swipe.current_frame = 0;
            }
        }
    } else {
        // Fallback default stick centers
        data->leftStickX = 128;
        data->leftStickY = 128;
        data->rightStickX = 128;
        data->rightStickY = 128;
    }

    // 3. Inject synthetic touchpad swipe data into ScePadTouchData
    if (s_swipe.gesture != SwipeGesture::None) {
        data->touchData.touchNum = 1;
        data->touchData.touch[0].id = 1;
        data->touchData.touch[0].x = 960; // Center X of 1920x940 touchpad

        float t = static_cast<float>(s_swipe.current_frame) / static_cast<float>(SyntheticSwipeState::TOTAL_FRAMES);

        if (s_swipe.gesture == SwipeGesture::SwipeUp) {
            // Upward trajectory: Y goes from 750 down to 150
            data->touchData.touch[0].y = static_cast<u16>(750.0f - (600.0f * t));
        } else if (s_swipe.gesture == SwipeGesture::SwipeDown) {
            // Downward trajectory: Y goes from 150 up to 750
            data->touchData.touch[0].y = static_cast<u16>(150.0f + (600.0f * t));
        } else if (s_swipe.gesture == SwipeGesture::TapCenter) {
            data->touchData.touch[0].y = 450;
        }
    } else {
        data->touchData.touchNum = 0;
    }

    // 4. Default orientation unit quaternion
    data->orientation.w = 1.0f;
    data->orientation.x = 0.0f;
    data->orientation.y = 0.0f;
    data->orientation.z = 0.0f;

    // Gravity vector in acceleration
    data->acceleration.y = -1.0f;

    return 0;
}

PS4_SYSV_ABI s32 scePadRead(s32 handle, ScePadData* data, s32 num) {
    if (!data || num < 1) return -1;
    return scePadReadState(handle, data);
}

PS4_SYSV_ABI s32 scePadSetLightBar(s32 handle, const void* color) {
    return 0;
}

PS4_SYSV_ABI s32 scePadSetVibration(s32 handle, const void* param) {
    return 0;
}

} // extern "C"

} // namespace Pad
