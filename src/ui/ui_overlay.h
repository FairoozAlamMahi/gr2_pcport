#pragma once
#include "common/types.h"

namespace UiOverlay {

enum class MenuState {
    PressStart,
    MainMenu,
    ControlsGuide,
    Playing
};

void Initialize();
void Update();
void Render(u32* display_buffer, u32 width, u32 height);
void SetState(MenuState state);
MenuState GetState();

} // namespace UiOverlay
