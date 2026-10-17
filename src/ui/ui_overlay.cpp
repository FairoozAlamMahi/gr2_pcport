#include "ui/ui_overlay.h"
#include "ui/font8x8.h"
#include "pad/pad_handler.h"
#include "common/logger.h"
#include <windows.h>
#include <cmath>
#include <cstring>
#include <string>
#include <algorithm>

namespace UiOverlay {

static MenuState s_state = MenuState::PressStart;
static int s_menu_index = 0;
static u64 s_frame_count = 0;
static u32 s_last_buttons = 0;
static int s_nav_cooldown = 0;

static const char* kMenuItems[] = {
    "START / NEW GAME",
    "CONTINUE / LOAD GAME",
    "CONTROLS & GAMEPAD GUIDE",
    "SETTINGS (60 FPS VULKAN 1.3)",
    "EXIT TO DESKTOP"
};
static constexpr int kNumMenuItems = 5;

void Initialize() {
    s_state = MenuState::PressStart;
    s_menu_index = 0;
    s_frame_count = 0;
    s_last_buttons = 0;
    s_nav_cooldown = 0;
    LOG_INFO("UI", "Interactive Title Screen and In-Game Menu Overlay initialized.");
}

void SetState(MenuState state) {
    s_state = state;
}

MenuState GetState() {
    return s_state;
}

static void DrawChar(u32* buf, u32 buf_w, u32 buf_h, int x, int y, char c, int scale, u32 color) {
    if (x + 8 * scale < 0 || x >= static_cast<int>(buf_w) || y + 8 * scale < 0 || y >= static_cast<int>(buf_h)) {
        return;
    }
    const uint8_t* glyph = UiFont::GetGlyph(c);
    for (int row = 0; row < 8; ++row) {
        uint8_t row_bits = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (row_bits & (0x80 >> col)) {
                for (int sy = 0; sy < scale; ++sy) {
                    int py = y + row * scale + sy;
                    if (py < 0 || py >= static_cast<int>(buf_h)) continue;
                    u32 row_offset = py * buf_w;
                    for (int sx = 0; sx < scale; ++sx) {
                        int px = x + col * scale + sx;
                        if (px >= 0 && px < static_cast<int>(buf_w)) {
                            buf[row_offset + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void DrawString(u32* buf, u32 buf_w, u32 buf_h, int x, int y, const char* str, int scale, u32 color, u32 shadow_color) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        char c = *str++;
        if (shadow_color != 0) {
            DrawChar(buf, buf_w, buf_h, cur_x + scale, y + scale, c, scale, shadow_color);
        }
        DrawChar(buf, buf_w, buf_h, cur_x, y, c, scale, color);
        cur_x += 8 * scale;
    }
}

static void DrawStringCentered(u32* buf, u32 buf_w, u32 buf_h, int y, const char* str, int scale, u32 color, u32 shadow_color) {
    if (!str) return;
    size_t len = std::strlen(str);
    int text_w = static_cast<int>(len * 8 * scale);
    int x = (static_cast<int>(buf_w) - text_w) / 2;
    DrawString(buf, buf_w, buf_h, x, y, str, scale, color, shadow_color);
}

static void DrawAlphaBox(u32* buf, u32 buf_w, u32 buf_h, int x, int y, int w, int h, u32 rgb_color, u8 alpha) {
    int x0 = std::clamp(x, 0, static_cast<int>(buf_w));
    int y0 = std::clamp(y, 0, static_cast<int>(buf_h));
    int x1 = std::clamp(x + w, 0, static_cast<int>(buf_w));
    int y1 = std::clamp(y + h, 0, static_cast<int>(buf_h));

    u32 a = static_cast<u32>(alpha) + (static_cast<u32>(alpha) >> 7);
    u32 inv_a = 256 - a;
    u32 src_rb = rgb_color & 0x00FF00FF;
    u32 src_g  = rgb_color & 0x0000FF00;

    for (int py = y0; py < y1; ++py) {
        u32* row = &buf[py * buf_w];
        for (int px = x0; px < x1; ++px) {
            u32 dst = row[px];
            u32 rb = (((dst & 0x00FF00FF) * inv_a + src_rb * a) >> 8) & 0x00FF00FF;
            u32 g  = (((dst & 0x0000FF00) * inv_a + src_g * a) >> 8) & 0x0000FF00;
            row[px] = 0xFF000000 | rb | g;
        }
    }
}

void Update() {
    s_frame_count++;
    if (s_nav_cooldown > 0) {
        s_nav_cooldown--;
    }

    u32 cur_buttons = Pad::GetCurrentButtons();

    // Also poll keyboard directly
    auto KeyPressed = [](int vk) -> bool {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    bool btn_up = (cur_buttons & 0x0010) || KeyPressed(VK_UP) || KeyPressed('W');
    bool btn_down = (cur_buttons & 0x0040) || KeyPressed(VK_DOWN) || KeyPressed('S');
    bool btn_confirm = (cur_buttons & 0x4000) || KeyPressed(VK_RETURN) || KeyPressed(VK_SPACE);
    bool btn_cancel = (cur_buttons & 0x2000) || KeyPressed(VK_BACK) || KeyPressed(VK_ESCAPE);

    bool confirm_pressed = btn_confirm && !(s_last_buttons & 0x4000);
    bool cancel_pressed = btn_cancel && !(s_last_buttons & 0x2000);

    if (s_state == MenuState::PressStart) {
        if (confirm_pressed || (cur_buttons & 0x0008)) { // Cross, Enter, or Options/Start
            s_state = MenuState::MainMenu;
            s_nav_cooldown = 15;
            Pad::InjectButtonPress(0x4000, 10); // Inject Cross into guest
            LOG_INFO("UI", "User pressed START -> Transitioned to Main Menu.");
        }
    } else if (s_state == MenuState::MainMenu) {
        if (s_nav_cooldown == 0) {
            if (btn_up) {
                s_menu_index = (s_menu_index + kNumMenuItems - 1) % kNumMenuItems;
                s_nav_cooldown = 12;
            } else if (btn_down) {
                s_menu_index = (s_menu_index + 1) % kNumMenuItems;
                s_nav_cooldown = 12;
            }
        }

        if (confirm_pressed && s_nav_cooldown == 0) {
            s_nav_cooldown = 15;
            switch (s_menu_index) {
                case 0: // START / NEW GAME
                    s_state = MenuState::Playing;
                    Pad::InjectButtonPress(0x4000, 20); // Inject Cross to advance game
                    LOG_INFO("UI", "Selected START GAME -> Entering active gameplay!");
                    break;
                case 1: // CONTINUE / LOAD GAME
                    s_state = MenuState::Playing;
                    Pad::InjectButtonPress(0x4000, 20);
                    LOG_INFO("UI", "Selected CONTINUE -> Loading latest save.");
                    break;
                case 2: // CONTROLS GUIDE
                    s_state = MenuState::ControlsGuide;
                    break;
                case 3: // SETTINGS
                    // In-game 60 FPS Vulkan settings info
                    s_nav_cooldown = 20;
                    break;
                case 4: // EXIT TO DESKTOP
                    LOG_INFO("UI", "Selected EXIT -> Graceful shutdown.");
                    ExitProcess(0);
                    break;
            }
        }

        if (cancel_pressed && s_nav_cooldown == 0) {
            s_state = MenuState::PressStart;
            s_nav_cooldown = 15;
        }
    } else if (s_state == MenuState::ControlsGuide) {
        if ((cancel_pressed || confirm_pressed) && s_nav_cooldown == 0) {
            s_state = MenuState::MainMenu;
            s_nav_cooldown = 15;
        }
    }

    s_last_buttons = cur_buttons;
}

void Render(u32* display_buffer, u32 width, u32 height) {
    if (!display_buffer || width != 1920 || height != 1080) return;

    Update();

    float time_val = static_cast<float>(s_frame_count) * 0.05f;
    float pulse = 0.5f + 0.5f * std::sin(time_val * 2.0f);

    if (s_state == MenuState::PressStart) {
        // 1. Semi-transparent header & footer cards
        DrawAlphaBox(display_buffer, width, height, 0, 80, 1920, 180, 0x0A0E1A, 140);
        DrawAlphaBox(display_buffer, width, height, 0, 1010, 1920, 70, 0x0A0E1A, 180);

        // 2. Stylized Title Logo
        DrawStringCentered(display_buffer, width, height, 110, "GRAVITY RUSH 2", 4, 0xFFFFD700, 0xFF1A1A1A);
        DrawStringCentered(display_buffer, width, height, 185, "PLAYSTATION 4 PC PORT  -  ARGON ENGINE 60 FPS", 2, 0xFFE0FFFF, 0xFF000000);

        // 3. Glowing Pulsing "PRESS START" Prompt
        DrawAlphaBox(display_buffer, width, height, 460, 690, 1000, 70, 0x050810, 190);
        u32 pulse_alpha = static_cast<u32>(180.0f + 75.0f * pulse);
        u32 r = 0xFF;
        u32 g = static_cast<u32>(215.0f + 40.0f * pulse);
        u32 b = static_cast<u32>(50.0f + 205.0f * pulse);
        u32 prompt_col = 0xFF000000 | (r << 16) | (g << 8) | b;
        (void)pulse_alpha;

        DrawStringCentered(display_buffer, width, height, 712, ">>> PRESS CROSS / A / ENTER TO START <<<", 2, prompt_col, 0xFF000000);

        // 4. Controller Detection & Status Indicator
        std::string pad_str = "[ CONTROLLER: PS4 DUALSHOCK 4 & XBOX 360 ACTIVE (SLOT 0) | VULKAN 1.3 | 60 FPS ]";
        if (!Pad::IsGamepadConnected()) {
            pad_str = "[ KEYBOARD & MOUSE ACTIVE | CONNECT CONTROLLER ANYTIME | VULKAN 1.3 | 60 FPS ]";
        }
        DrawStringCentered(display_buffer, width, height, 1035, pad_str.c_str(), 1, 0xFF88FF88, 0xFF000000);
    } else if (s_state == MenuState::MainMenu) {
        // Semi-transparent center menu window
        DrawAlphaBox(display_buffer, width, height, 540, 200, 840, 680, 0x0C1224, 230);
        DrawAlphaBox(display_buffer, width, height, 550, 210, 820, 660, 0x16223D, 180);

        // Title in Menu
        DrawStringCentered(display_buffer, width, height, 240, "GRAVITY RUSH 2", 3, 0xFFFFD700, 0xFF000000);
        DrawStringCentered(display_buffer, width, height, 290, "- MAIN MENU -", 2, 0xFF00FFFF, 0xFF000000);

        // Menu Items
        int start_y = 370;
        for (int i = 0; i < kNumMenuItems; ++i) {
            int item_y = start_y + i * 75;
            bool is_selected = (i == s_menu_index);

            if (is_selected) {
                // Highlight box with cyan/blue accent
                DrawAlphaBox(display_buffer, width, height, 580, item_y - 10, 760, 50, 0x245596, 210);
                std::string item_text = std::string(">> ") + kMenuItems[i] + " <<";
                DrawStringCentered(display_buffer, width, height, item_y + 4, item_text.c_str(), 2, 0xFFFFFFFF, 0xFF000000);
            } else {
                DrawStringCentered(display_buffer, width, height, item_y + 4, kMenuItems[i], 2, 0xFFA0B4CD, 0xFF000000);
            }
        }

        // Navigation Footer
        DrawAlphaBox(display_buffer, width, height, 0, 1010, 1920, 70, 0x0A0E1A, 200);
        DrawStringCentered(display_buffer, width, height, 1035, "< D-PAD / ARROWS: NAVIGATE >   < CROSS / A / ENTER: SELECT >   < CIRCLE / B / ESC: BACK >", 1, 0xFFE0E0E0, 0xFF000000);
    } else if (s_state == MenuState::ControlsGuide) {
        // Controls Guide Dialog
        DrawAlphaBox(display_buffer, width, height, 360, 120, 1200, 840, 0x0A0E1E, 240);
        DrawStringCentered(display_buffer, width, height, 150, "CONTROLLER & KEYBOARD GUIDE", 3, 0xFFFFD700, 0xFF000000);

        int gy = 230;
        int gx_ps4 = 420;
        int gx_xb = 780;
        int gx_pc = 1100;
        int gx_act = 1320;

        DrawString(display_buffer, width, height, gx_ps4, gy, "[PS4 DualShock 4]", 1, 0xFF00FFFF, 0);
        DrawString(display_buffer, width, height, gx_xb, gy, "[Xbox 360 / XInput]", 1, 0xFF88FF88, 0);
        DrawString(display_buffer, width, height, gx_pc, gy, "[Keyboard]", 1, 0xFFFFFF00, 0);
        DrawString(display_buffer, width, height, gx_act, gy, "[Action]", 1, 0xFFFFFFFF, 0);

        gy += 35;
        DrawAlphaBox(display_buffer, width, height, 400, gy, 1120, 2, 0xFFFFFF, 120);
        gy += 20;

        struct ControlRow {
            const char* ps4;
            const char* xb;
            const char* pc;
            const char* act;
        };
        static const ControlRow kRows[] = {
            { "Cross",              "A",              "Enter / Space",   "Confirm / Jump" },
            { "Circle",             "B",              "Backspace / Esc", "Cancel / Evade" },
            { "Square",             "X",              "E / F / LMB",     "Gravity Kick / Attack" },
            { "Triangle",           "Y",              "Q / R",           "Special Attack" },
            { "R2 Trigger",         "RT Trigger",     "Shift / V",       "Gravity Shift / Free Fall" },
            { "L2 Trigger",         "LT Trigger",     "C",               "Gravity Slide" },
            { "R1",                 "RB",             "Shift",           "Reset Gravity" },
            { "LB + D-Pad Up",      "LB + D-Pad Up",  "Key 1",           "Lunar Style (Light Gravity)" },
            { "LB + D-Pad Down",    "LB + D-Pad Down","Key 2",           "Jupiter Style (Heavy Gravity)" },
            { "LB + D-Pad L/R",     "LB + D-Pad L/R", "Key 3",           "Normal Gravity Style" },
            { "Left Stick",         "Left Stick",     "W / A / S / D",   "Movement" },
            { "Right Stick",        "Right Stick",    "Mouse / Arrows",  "Camera / Gyro Aim" }
        };

        for (const auto& row : kRows) {
            DrawString(display_buffer, width, height, gx_ps4, gy, row.ps4, 1, 0xFFFFFFFF, 0);
            DrawString(display_buffer, width, height, gx_xb, gy, row.xb, 1, 0xFF88FF88, 0);
            DrawString(display_buffer, width, height, gx_pc, gy, row.pc, 1, 0xFFFFFF00, 0);
            DrawString(display_buffer, width, height, gx_act, gy, row.act, 1, 0xFF00FFFF, 0);
            gy += 40;
        }

        DrawStringCentered(display_buffer, width, height, 900, "PRESS CIRCLE / B / ESC TO RETURN TO MENU", 2, 0xFFFFD700, 0xFF000000);
    } else if (s_state == MenuState::Playing) {
        // Discreet in-game HUD indicator (fade out or top-right badge)
        DrawAlphaBox(display_buffer, width, height, 1420, 20, 480, 40, 0x050810, 160);
        DrawString(display_buffer, width, height, 1440, 32, "GRAVITY RUSH 2 PC  |  60 FPS  |  VULKAN 1.3", 1, 0xFF88FF88, 0xFF000000);
    }
}

} // namespace UiOverlay
