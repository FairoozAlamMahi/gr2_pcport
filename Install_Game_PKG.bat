@echo off
setlocal EnableDelayedExpansion
title Gravity Rush 2 PC Port - Game Installer
color 0b

echo ====================================================================
echo               GRAVITY RUSH 2 PC PORT - GAME INSTALLER
echo ====================================================================
echo.
echo This tool will install and configure your legal Gravity Rush 2 PKG
echo files into the port directory.
echo.

set SCRIPT_DIR=%~dp0
set EXTRACTOR=%SCRIPT_DIR%tools\pkg_extractor.exe
set GAME_DIR=%SCRIPT_DIR%game
set TARGET_EBOOT=%GAME_DIR%\eboot.bin

if not exist "%EXTRACTOR%" (
    echo [ERROR] Could not find pkg_extractor.exe at:
    echo   "%EXTRACTOR%"
    pause
    exit /b 1
)

if not exist "%GAME_DIR%" (
    mkdir "%GAME_DIR%"
)

:: Check if game is already installed
if exist "%TARGET_EBOOT%" (
    echo [INFO] Game executable already detected at "%TARGET_EBOOT%".
    echo You can launch the game directly using GravityRush2.exe!
    echo.
    set /p REINSTALL="Do you want to re-install/update anyway? (y/n): "
    if /i not "!REINSTALL!"=="y" (
        goto :launch_prompt
    )
)

echo Scanning for PS4 PKG files in:
echo   "%SCRIPT_DIR%"
echo.

set PKG_FOUND=0
for %%f in ("%SCRIPT_DIR%*.pkg" "%SCRIPT_DIR%packages\*.pkg" "%SCRIPT_DIR%pkg_drop\*.pkg") do (
    set /a PKG_FOUND+=1
    echo [FOUND] %%~nxf
)

if %PKG_FOUND% EQU 0 (
    echo [NOTICE] No .pkg files were found in this directory.
    echo.
    echo Please place your Gravity Rush 2 PKG files:
    echo   1. Base Game PKG (e.g. CUSA04943...Game_Full.pkg)
    echo   2. Update v1.11 PKG (e.g. CUSA04943...Patch_v1.11.pkg)
    echo into this folder:
    echo   "%SCRIPT_DIR%"
    echo.
    echo Alternatively, drag and drop a PKG file directly onto this .bat file!
    echo.
    pause
    exit /b 0
)

echo.
echo Installing PKG components into game directory...
echo --------------------------------------------------------------------

for %%f in ("%SCRIPT_DIR%*.pkg" "%SCRIPT_DIR%packages\*.pkg" "%SCRIPT_DIR%pkg_drop\*.pkg") do (
    echo.
    echo [EXTRACTING] Processing "%%~nxf"...
    "%EXTRACTOR%" "%%f" "%GAME_DIR%"
    if !ERRORLEVEL! NEQ 0 (
        echo [WARNING] Extraction of %%~nxf returned code !ERRORLEVEL!.
    ) else (
        echo [SUCCESS] Extracted %%~nxf cleanly.
    )
)

:: If extracted into a subfolder like CUSA04943, move or link files up to game\
if not exist "%TARGET_EBOOT%" (
    for /d %%d in ("%GAME_DIR%\CUSA*" "%GAME_DIR%\CUSA04943*") do (
        if exist "%%d\eboot.bin" (
            echo [INFO] Organizing game structure from %%~nxd...
            xcopy "%%d\*" "%GAME_DIR%\" /E /H /Y /Q >nul 2>&1
        )
    )
)

if exist "%TARGET_EBOOT%" (
    echo.
    echo ====================================================================
    echo               INSTALLATION COMPLETE SUCCESSFULLY!
    echo ====================================================================
    echo The game executable was verified at:
    echo   "%TARGET_EBOOT%"
    echo.
    echo You are now ready to play Gravity Rush 2 at 60+ FPS!
    echo.
) else (
    echo.
    echo [WARNING] Could not locate eboot.bin in "%GAME_DIR%".
    echo Please verify that your PKG files are valid and contain eboot.bin.
    echo.
)

:launch_prompt
echo Double-click GravityRush2.exe anytime to play!
echo.
set /p LAUNCH_NOW="Would you like to start the game now? (y/n): "
if /i "!LAUNCH_NOW!"=="y" (
    start "" "%SCRIPT_DIR%GravityRush2.exe"
)
exit /b 0
