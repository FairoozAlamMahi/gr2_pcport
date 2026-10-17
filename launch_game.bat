@echo off
title Gravity Rush 2 PC Port (Argon Engine) - 60 FPS
cd /d "%~dp0"
echo ===============================================================
echo        GRAVITY RUSH 2 - NATIVE PC PORT (gr2port)
echo        60 FPS Native Vulkan 1.3 - Dual Gamepad Active
echo ===============================================================
echo.
echo Controllers Active:
echo   - PS4 DualShock 4 (via DS4Windows)
echo   - Xbox 360 Gamepad (XInput Slot 0-3)
echo.
echo Controls:
echo   - Cross / A / Enter: Confirm / Select / Jump
echo   - Circle / B / Backspace: Cancel
echo   - R2 / RT: Gravity Shift
echo   - L2 / LT: Gravity Slide
echo   - Square / X (in air): Gravity Kick
echo   - LB + D-Pad Up (or 1): Lunar Style
echo   - LB + D-Pad Down (or 2): Jupiter Style
echo   - LB + D-Pad Left/Right (or 3): Normal Style
echo Launching Retail Game Engine (Direct Vulkan 60 FPS)...
echo ===============================================================

if exist "..\launch_game.bat" (
    call "..\launch_game.bat"
) else (
    echo [ERROR] Could not find root launcher at ..\launch_game.bat!
    pause
)
