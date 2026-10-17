#include "pad/pad_handler.h"
#include "common/logger.h"
#include <xinput.h>
#include <cmath>
#include <chrono>
#include <atomic>
#include <string>
#include <algorithm>

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
    u8 touch_id{1};
    static constexpr int TOTAL_FRAMES = 8;
};

static SyntheticSwipeState s_swipe{};
static POINT s_last_mouse_pos{};
static bool s_has_mouse_init = false;
static float s_mouse_delta_x = 0.0f;
static float s_mouse_delta_y = 0.0f;

static std::atomic<u32> s_injected_buttons{0};
static std::atomic<int> s_injected_frames{0};
static std::atomic<u32> s_current_buttons{0};
static std::atomic<bool> s_gamepad_connected{false};
static std::atomic<int> s_gamepad_slot{-1};

static inline std::string ToHex(u64 val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
    return std::string(buf);
}

void InjectButtonPress(u32 buttons, int duration_frames) {
    s_injected_buttons.store(buttons, std::memory_order_relaxed);
    s_injected_frames.store(duration_frames, std::memory_order_relaxed);
    LOG_INFO("PAD", "Injected button press 0x" + ToHex(buttons) + " for " + std::to_string(duration_frames) + " frames.");
}

u32 GetCurrentButtons() {
    return s_current_buttons.load(std::memory_order_relaxed);
}

bool IsGamepadConnected() {
    return s_gamepad_connected.load(std::memory_order_relaxed);
}

int GetActiveGamepadSlot() {
    return s_gamepad_slot.load(std::memory_order_relaxed);
}

void Initialize() {
    LOG_INFO("PAD", "Initializing Gamepad & Synthetic Touchpad Swipe Engine...");
    GetCursorPos(&s_last_mouse_pos);
    s_has_mouse_init = true;
}

void UpdateInput() {
    // Process host mouse movement for camera and gyro fallback
    POINT cur_mouse;
    GetCursorPos(&cur_mouse);
    if (s_has_mouse_init) {
        s_mouse_delta_x = static_cast<float>(cur_mouse.x - s_last_mouse_pos.x);
        s_mouse_delta_y = static_cast<float>(cur_mouse.y - s_last_mouse_pos.y);
    } else {
        s_mouse_delta_x = 0.0f;
        s_mouse_delta_y = 0.0f;
    }
    s_last_mouse_pos = cur_mouse;

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

PS4_SYSV_ABI s32 scePadInit() {
    Initialize();
    return 0;
}

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
    auto now = std::chrono::steady_clock::now();
    data->timestamp = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());

    static std::atomic<u64> s_pad_reads{0};
    u64 reads = s_pad_reads.fetch_add(1, std::memory_order_relaxed);
    if (reads <= 5 || reads % 180 == 0) {
        LOG_INFO("PAD", "scePadReadState: sampling inputs (read #" + std::to_string(reads) + ")");
    }

    // Apply injected programmatic buttons (from UI overlay selection)
    int inj_frames = s_injected_frames.load(std::memory_order_relaxed);
    if (inj_frames > 0) {
        data->buttons |= s_injected_buttons.load(std::memory_order_relaxed);
        s_injected_frames.fetch_sub(1, std::memory_order_relaxed);
    }

    static bool s_auto_input_enabled = (GetEnvironmentVariableA("GR2_AUTO_INPUT", nullptr, 0) > 0);
    if (s_auto_input_enabled) {
        if (reads > 60 && (reads % 60 < 10)) {
            data->buttons |= 0x4000; // Cross (Enter / A button)
        }
    }

    // Standard Orbis SDK button bitmasks
    constexpr u32 ORBIS_PAD_L3        = 0x00000002;
    constexpr u32 ORBIS_PAD_R3        = 0x00000004;
    constexpr u32 ORBIS_PAD_OPTIONS   = 0x00000008;
    constexpr u32 ORBIS_PAD_UP        = 0x00000010;
    constexpr u32 ORBIS_PAD_RIGHT     = 0x00000020;
    constexpr u32 ORBIS_PAD_DOWN      = 0x00000040;
    constexpr u32 ORBIS_PAD_LEFT      = 0x00000080;
    constexpr u32 ORBIS_PAD_L2        = 0x00000100;
    constexpr u32 ORBIS_PAD_R2        = 0x00000200;
    constexpr u32 ORBIS_PAD_L1        = 0x00000400;
    constexpr u32 ORBIS_PAD_R1        = 0x00000800;
    constexpr u32 ORBIS_PAD_TRIANGLE  = 0x00001000;
    constexpr u32 ORBIS_PAD_CIRCLE    = 0x00002000;
    constexpr u32 ORBIS_PAD_CROSS     = 0x00004000;
    constexpr u32 ORBIS_PAD_SQUARE    = 0x00008000;
    constexpr u32 ORBIS_PAD_TOUCHPAD  = 0x00100000;

    // 1. Read XInput Controller across all 4 potential slots (supporting PS4 DS4Windows & Xbox 360)
    XINPUT_STATE xi_state{};
    DWORD dwResult = ERROR_DEVICE_NOT_CONNECTED;
    static int s_active_slot = -1;
    int current_slot = -1;

    for (DWORD i = 0; i < 4; ++i) {
        if (XInputGetState(i, &xi_state) == ERROR_SUCCESS) {
            dwResult = ERROR_SUCCESS;
            current_slot = static_cast<int>(i);
            break;
        }
    }

    if (current_slot != s_active_slot) {
        if (current_slot >= 0) {
            LOG_INFO("PAD", "Gamepad connected on XInput slot " + std::to_string(current_slot));
        } else if (s_active_slot >= 0) {
            LOG_INFO("PAD", "Gamepad disconnected, keyboard fallback active");
        }
        s_active_slot = current_slot;
    }

    static bool s_lb_used_for_combo = false;

    if (dwResult == ERROR_SUCCESS) {
        WORD wButtons = xi_state.Gamepad.wButtons;

        u32 ps4_btn = 0;
        if (wButtons & XINPUT_GAMEPAD_A) ps4_btn |= ORBIS_PAD_CROSS;
        if (wButtons & XINPUT_GAMEPAD_B) ps4_btn |= ORBIS_PAD_CIRCLE;
        if (wButtons & XINPUT_GAMEPAD_X) ps4_btn |= ORBIS_PAD_SQUARE;
        if (wButtons & XINPUT_GAMEPAD_Y) ps4_btn |= ORBIS_PAD_TRIANGLE;

        if (wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ps4_btn |= ORBIS_PAD_R1;

        // Analog triggers: map digital button bit and exact analog byte value
        data->analogButtons[0] = xi_state.Gamepad.bLeftTrigger;
        data->analogButtons[1] = xi_state.Gamepad.bRightTrigger;

        if (xi_state.Gamepad.bLeftTrigger > 30)  ps4_btn |= ORBIS_PAD_L2;
        if (xi_state.Gamepad.bRightTrigger > 30) ps4_btn |= ORBIS_PAD_R2;

        if (wButtons & XINPUT_GAMEPAD_START) ps4_btn |= ORBIS_PAD_OPTIONS;
        if (wButtons & XINPUT_GAMEPAD_BACK)  ps4_btn |= ORBIS_PAD_TOUCHPAD; // PS4 Touchpad Click
        if (wButtons & XINPUT_GAMEPAD_LEFT_THUMB)  ps4_btn |= ORBIS_PAD_L3;
        if (wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ps4_btn |= ORBIS_PAD_R3;

        // 2. Synthetic Touchpad Swipe Engine for Gravity Style Switching:
        // LB + D-Pad Up   -> Lunar Style (Swipe Up)
        // LB + D-Pad Down -> Jupiter Style (Swipe Down)
        // LB + D-Pad Left/Right -> Normal Style (Center Tap)
        bool lb_held = (wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        bool dpad_any = (wButtons & (XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT)) != 0;

        if (lb_held && dpad_any) {
            s_lb_used_for_combo = true;
            if ((wButtons & XINPUT_GAMEPAD_DPAD_UP) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::SwipeUp;
                s_swipe.current_frame = 0;
                s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
                LOG_INFO("PAD", "Activated Lunar Style synthetic swipe (Up)");
            } else if ((wButtons & XINPUT_GAMEPAD_DPAD_DOWN) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::SwipeDown;
                s_swipe.current_frame = 0;
                s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
                LOG_INFO("PAD", "Activated Jupiter Style synthetic swipe (Down)");
            } else if ((wButtons & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT)) && s_swipe.gesture == SwipeGesture::None) {
                s_swipe.gesture = SwipeGesture::TapCenter;
                s_swipe.current_frame = 0;
                s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
                LOG_INFO("PAD", "Activated Normal Style synthetic tap (Center)");
            }
        } else {
            if (!lb_held) {
                s_lb_used_for_combo = false;
            }
            // Pass through L1 only if not participating in a style-switch combo
            if (lb_held && !s_lb_used_for_combo && s_swipe.gesture == SwipeGesture::None) {
                ps4_btn |= ORBIS_PAD_L1;
            }
            // Pass through D-Pad only when not in a combo
            if (!s_lb_used_for_combo) {
                if (wButtons & XINPUT_GAMEPAD_DPAD_UP)    ps4_btn |= ORBIS_PAD_UP;
                if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ps4_btn |= ORBIS_PAD_RIGHT;
                if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ps4_btn |= ORBIS_PAD_DOWN;
                if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ps4_btn |= ORBIS_PAD_LEFT;
            }
        }

        data->buttons = ps4_btn;

        // Radial deadzone processing to prevent stick drift on Xbox 360 & DualShock controllers
        constexpr float DEADZONE_LEFT  = 7849.0f; // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
        constexpr float DEADZONE_RIGHT = 8689.0f; // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

        auto ApplyDeadzone = [](SHORT raw_x, SHORT raw_y, float deadzone, u8& out_x, u8& out_y) {
            float fx = static_cast<float>(raw_x);
            float fy = static_cast<float>(raw_y);
            float mag = std::sqrt(fx * fx + fy * fy);
            if (mag > deadzone) {
                float norm_mag = std::min(1.0f, (mag - deadzone) / (32767.0f - deadzone));
                float nx = (fx / mag) * norm_mag;
                float ny = (fy / mag) * norm_mag;
                out_x = static_cast<u8>(std::clamp(static_cast<int>(128.0f + nx * 127.0f), 0, 255));
                out_y = static_cast<u8>(std::clamp(static_cast<int>(128.0f - ny * 127.0f), 0, 255));
            } else {
                out_x = 128;
                out_y = 128;
            }
        };

        ApplyDeadzone(xi_state.Gamepad.sThumbLX, xi_state.Gamepad.sThumbLY, DEADZONE_LEFT, data->leftStickX, data->leftStickY);
        ApplyDeadzone(xi_state.Gamepad.sThumbRX, xi_state.Gamepad.sThumbRY, DEADZONE_RIGHT, data->rightStickX, data->rightStickY);
    } else {
        // Fallback default stick centers
        data->leftStickX = 128;
        data->leftStickY = 128;
        data->rightStickX = 128;
        data->rightStickY = 128;
    }

    // 3. Full PC Keyboard & Mouse fallback engine
    auto IsKeyDown = [](int vk) -> bool {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    u32 kb_btn = 0;
    if (IsKeyDown(VK_RETURN) || IsKeyDown(VK_SPACE)) kb_btn |= ORBIS_PAD_CROSS;   // Enter / Space: Cross
    if (IsKeyDown(VK_ESCAPE) || IsKeyDown(VK_TAB))   kb_btn |= ORBIS_PAD_OPTIONS; // Esc / Tab: Pause / Options
    if (IsKeyDown(VK_BACK) || IsKeyDown(VK_RBUTTON)) kb_btn |= ORBIS_PAD_CIRCLE;  // Backspace / RMB: Circle
    if (IsKeyDown('E') || IsKeyDown('F'))            kb_btn |= ORBIS_PAD_SQUARE;  // E / F: Square
    if (IsKeyDown('Q') || IsKeyDown('R'))            kb_btn |= ORBIS_PAD_TRIANGLE;// Q / R: Triangle
    if (IsKeyDown(VK_SHIFT))                         kb_btn |= ORBIS_PAD_R1;      // Shift: R1 (Gravity Shift/Kick)
    if (IsKeyDown(VK_CONTROL))                       kb_btn |= ORBIS_PAD_L1;      // Ctrl: L1 (Cancel Gravity)

    // Keyboard triggers for Gravity Slide and Evade
    if (IsKeyDown('C')) {
        kb_btn |= ORBIS_PAD_L2;
        data->analogButtons[0] = 255;
    }
    if (IsKeyDown('V')) {
        kb_btn |= ORBIS_PAD_R2;
        data->analogButtons[1] = 255;
    }

    // Touchpad Click hotkey (T or Middle Mouse)
    if (IsKeyDown('T') || IsKeyDown(VK_MBUTTON)) {
        kb_btn |= ORBIS_PAD_TOUCHPAD;
    }

    // D-Pad navigation on PC (NumPad arrows or alt controls)
    if (IsKeyDown(VK_NUMPAD8)) kb_btn |= ORBIS_PAD_UP;
    if (IsKeyDown(VK_NUMPAD6)) kb_btn |= ORBIS_PAD_RIGHT;
    if (IsKeyDown(VK_NUMPAD2)) kb_btn |= ORBIS_PAD_DOWN;
    if (IsKeyDown(VK_NUMPAD4)) kb_btn |= ORBIS_PAD_LEFT;

    // Synthetic touchpad gesture hotkeys on PC:
    // Key '1' -> Lunar Style (Swipe Up)
    // Key '2' -> Jupiter Style (Swipe Down)
    // Key '3' -> Normal Style (Tap Center)
    if (IsKeyDown('1') && s_swipe.gesture == SwipeGesture::None) {
        s_swipe.gesture = SwipeGesture::SwipeUp;
        s_swipe.current_frame = 0;
        s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
        LOG_INFO("PAD", "Activated Lunar Style via PC key '1'");
    } else if (IsKeyDown('2') && s_swipe.gesture == SwipeGesture::None) {
        s_swipe.gesture = SwipeGesture::SwipeDown;
        s_swipe.current_frame = 0;
        s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
        LOG_INFO("PAD", "Activated Jupiter Style via PC key '2'");
    } else if (IsKeyDown('3') && s_swipe.gesture == SwipeGesture::None) {
        s_swipe.gesture = SwipeGesture::TapCenter;
        s_swipe.current_frame = 0;
        s_swipe.touch_id = (s_swipe.touch_id % 250) + 1;
        LOG_INFO("PAD", "Activated Normal Style via PC key '3'");
    }

    // WASD movement mapping when no analog stick active
    if (data->leftStickX == 128 && data->leftStickY == 128) {
        int lx = 128, ly = 128;
        if (IsKeyDown('W')) ly -= 120;
        if (IsKeyDown('S')) ly += 120;
        if (IsKeyDown('A')) lx -= 120;
        if (IsKeyDown('D')) lx += 120;
        data->leftStickX = static_cast<u8>(std::clamp(lx, 0, 255));
        data->leftStickY = static_cast<u8>(std::clamp(ly, 0, 255));
    }

    // Mouse / Arrows / IJKL Camera mapping when no gamepad right stick active
    if (data->rightStickX == 128 && data->rightStickY == 128) {
        int rx = 128, ry = 128;
        if (IsKeyDown(VK_UP) || IsKeyDown('I'))    ry -= 120;
        if (IsKeyDown(VK_DOWN) || IsKeyDown('K'))  ry += 120;
        if (IsKeyDown(VK_LEFT) || IsKeyDown('J'))  rx -= 120;
        if (IsKeyDown(VK_RIGHT) || IsKeyDown('L')) rx += 120;

        // Seamless mouse delta camera control
        if (std::abs(s_mouse_delta_x) > 0.5f || std::abs(s_mouse_delta_y) > 0.5f) {
            rx += static_cast<int>(std::clamp(s_mouse_delta_x * 8.0f, -120.0f, 120.0f));
            ry += static_cast<int>(std::clamp(s_mouse_delta_y * 8.0f, -120.0f, 120.0f));
        }

        data->rightStickX = static_cast<u8>(std::clamp(rx, 0, 255));
        data->rightStickY = static_cast<u8>(std::clamp(ry, 0, 255));
    }

    data->buttons |= kb_btn;

    // 4. Inject synthetic touchpad swipe data into ScePadTouchData
    if (s_swipe.gesture != SwipeGesture::None) {
        data->touchData.touchNum = 1;
        data->touchData.touch[0].id = s_swipe.touch_id;
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

    // Advance synthetic swipe frame & mouse state
    UpdateInput();

    // 5. Default orientation unit quaternion
    data->orientation.w = 1.0f;
    data->orientation.x = 0.0f;
    data->orientation.y = 0.0f;
    data->orientation.z = 0.0f;

    // Mouse delta converted into angular velocity for gyro aiming
    if (std::abs(s_mouse_delta_x) > 0.1f || std::abs(s_mouse_delta_y) > 0.1f) {
        data->angularVelocity.x = s_mouse_delta_y * 0.05f;
        data->angularVelocity.y = s_mouse_delta_x * 0.05f;
        data->angularVelocity.z = 0.0f;
    }

    // Gravity vector in acceleration
    data->acceleration.y = -1.0f;

    // Record atomic state for UI and systems
    s_current_buttons.store(data->buttons, std::memory_order_relaxed);
    s_gamepad_connected.store(dwResult == ERROR_SUCCESS, std::memory_order_relaxed);
    s_gamepad_slot.store(current_slot, std::memory_order_relaxed);

    static u32 s_last_logged_btn = 0;
    if (data->buttons != 0 && data->buttons != s_last_logged_btn) {
        LOG_INFO("PAD", "Input detected: 0x" + ToHex(data->buttons) + 
                 ((data->buttons & ORBIS_PAD_CROSS) ? " [CROSS / A]" : "") +
                 ((data->buttons & ORBIS_PAD_CIRCLE) ? " [CIRCLE / B]" : "") +
                 ((data->buttons & ORBIS_PAD_OPTIONS) ? " [OPTIONS / START]" : ""));
        s_last_logged_btn = data->buttons;
    } else if (data->buttons == 0) {
        s_last_logged_btn = 0;
    }

    return 0;
}

PS4_SYSV_ABI s32 scePadRead(s32 handle, ScePadData* data, s32 num) {
    if (!data || num < 1) return -1;
    return scePadReadState(handle, data);
}

PS4_SYSV_ABI s32 scePadGetControllerInformation(s32 handle, ScePadControllerInformation* info) {
    (void)handle;
    if (!info) return -1;
    std::memset(info, 0, sizeof(ScePadControllerInformation));
    info->touchPixelDensity = 40.0f;
    info->touchResolutionX = 1920;
    info->touchResolutionY = 940;
    info->stickInfo[0] = 0;
    info->stickInfo[1] = 0;
    info->stickInfo[2] = 1;
    info->stickInfo[3] = 1;
    info->connected = 1;
    info->connectionType = 0;
    info->connectedCount = 1;
    info->deviceClass = 0;
    return 0;
}

PS4_SYSV_ABI s32 scePadResetOrientation(s32 handle) {
    (void)handle;
    return 0;
}

PS4_SYSV_ABI s32 scePadSetTiltCorrectionState(s32 handle, s32 state) {
    (void)handle;
    (void)state;
    return 0;
}

PS4_SYSV_ABI s32 scePadSetLightBar(s32 handle, const void* color) {
    (void)handle;
    (void)color;
    return 0;
}

PS4_SYSV_ABI s32 scePadSetVibration(s32 handle, const void* param) {
    (void)handle;
    XINPUT_VIBRATION xv{};
    if (param) {
        const auto* vib = reinterpret_cast<const ScePadVibrationParam*>(param);
        xv.wLeftMotorSpeed = static_cast<WORD>(vib->largeMotor * 257);
        xv.wRightMotorSpeed = static_cast<WORD>(vib->smallMotor * 257);
    }
    for (DWORD i = 0; i < 4; ++i) {
        XInputSetState(i, &xv);
    }
    return 0;
}

} // extern "C"

} // namespace Pad
