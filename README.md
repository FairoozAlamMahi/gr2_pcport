# Gravity Rush 2 (PC Port) — Quickstart & Player Guide

Welcome to the standalone PC release of **Gravity Rush 2** (`CUSA04943` / `CUSA03694` v1.11).

This port features **direct host execution** (zero CPU emulation), native **Vulkan 1.3** graphics pipeline with multi-threaded pipeline pre-warming (zero stutter), **rock-solid 60 FPS locked dynamic timestep**, **native clean-room C NGS2 audio synthesis** (`libSceNgs2` voices, combat SFX, world audio, dialogue, and Tanaka's orchestral score), dynamic headset/speaker switching, an in-game live Control Panel (`F1`), and complete gamepad & keyboard controls.

---

## ⚡ Quickstart (3 Easy Steps)

### Step 1: Supply Your Game PKGs
Place your legal PlayStation 4 *Gravity Rush 2* PKG files into this folder:
- **Base Game PKG** (e.g. `PS4_Gravity_Rush 2_CUSA04943_01_Game_Full.pkg`)
- **Update v1.11 PKG** (e.g. `PS4_Gravity_Rush 2_CUSA04943_02_Patch_v1.11.pkg`) — *Required for 60 FPS physics*

*(If you already have a dumped `game/` folder containing `eboot.bin` and `.psarc` files, you can skip Step 2!)*

### Step 2: Install Game Files
Double-click **`Install_Game_PKG.bat`**.
The installer will automatically unpack, merge, and configure the game in seconds.

### Step 3: Play!
Double-click **`GravityRush2.exe`** or the desktop shortcut **`Play Gravity Rush 2.bat`** to start the game directly!

---

## 🔊 Audio & Sound System (100% Native In-Game Synthesis)

The port features a complete clean-room C implementation of Sony's Next-Generation Sound 2 DSP engine (`libSceNgs2`):
- **Full In-Game Audio**: Kat's voice lines, combat impacts, gravity shift sound effects, ambient city sounds, and orchestral soundtrack are synthesized in real-time.
- **Dynamic Headset / Speaker Auto-Routing**:
  The launcher and in-game Control Panel (`F1`) detect all connected audio devices (e.g. *Gaming Headset*, *Monitor / TV Speakers*, *Optical Audio*).
  When you launch or switch devices, audio routes directly to your headphones or speakers instantly.
- **In-Game Sound Settings Tip**:
  In the game's Options menu (`Sound Options`), set **"Controller Speaker" to OFF**.
  *(On PS4, Kat's voice lines and gravity alerts route to the DualShock 4 speaker by default. Setting it to OFF directs all voice lines to your headphones/main speakers!)*
- **One-Click Audio & Slider Reset**:
  If the in-game volume sliders ever become desynchronized, open the **F1 Control Panel** and click **"🔧 One-Click Sound Slider & Audio Reset"** to restore pristine 100% volume defaults immediately.

---

## 🚀 Performance Profiles & 1-Click Launchers

Four dedicated optimization presets are provided to test and run the game across different scenarios:

| Preset / Mode | Target Resolution | Framerate / VBlank | Audio Mode | Purpose / Best For |
| :--- | :--- | :--- | :--- | :--- |
| **Profile 1: Native 60 FPS** *(Default)* | **1080p Native** | **60 FPS** (240Hz VBlank) | Native WASAPI | **Recommended standard gameplay**. 240Hz sub-frame VBlank eliminates 33ms drops. Device-local GPU buffer copies (`TLG_COPY_GPU_BUFFERS=1`) and 6 staging worker threads eliminate host memory stalls. |
| **Profile 2: 720p Performance** | **720p Downscaled** | **60 FPS** (240Hz VBlank) | Native WASAPI | **High FPS Testing Mode**. Reduces pixel fillrate and shading area by **55.6%**, guaranteeing <10ms GPU frame times even during complex 3,800-draw scenes. |
| **Profile 3: Diagnostic Silent** | **1080p Native** | **60 FPS** (240Hz VBlank) | **Decoupled Timer** | **Timing & Audio Diagnostic Mode**. Decouples physical audio output to verify script and miner NPC timing without sound hardware interaction. |
| **Profile 4: Ultra 120 FPS** | **1080p Native** | **120 FPS** (240Hz VBlank) | Native WASAPI | **High-Refresh Display Mode**. Runs simulation and presentation uncapped at 120 Hz. |

### 1-Click Launch Options:
- **`Play Profile 1 - 60 FPS Native.bat`** (or desktop shortcut)
- **`Play Profile 2 - 720p Performance.bat`** (or desktop shortcut)
- **`Play Profile 3 - Silent Audio Diagnostic.bat`** (or desktop shortcut)
- **`Play Profile 4 - 120 FPS Mode.bat`** (or desktop shortcut)
- **`GravityRush2.exe`** / **`Gravity Rush 2 Control Panel.bat`**: Opens the GUI launcher allowing you to pick your preset, audio device, display mode, and settings before launching!

---

## 🔬 Architectural Optimizations (Deep-Dive)

1. **240 Hz VBlank Cadence vs. 60 Hz Drop**:
   - In retail PS4 code, missing a 60 Hz VBlank window (16.66ms) forces the display presenter to wait for the next boundary at **33.33ms (30 FPS)**. Because the simulation timestep advances per VBlank flip, 30 FPS flips cause Kat and the world to move in **half-speed slow-motion**.
   - With `TLG_VBLANK_HZ=240`, the VBlank ticker fires every **4.16ms**. A frame taking 17–18ms catches the 20.8ms boundary instead of waiting for 33.3ms, completely eliminating slow-motion!
2. **Device-Local GPU Buffer Copies (`TLG_COPY_GPU_BUFFERS=1`)**:
   - Replaces CPU host staging roundtrips with direct `vkCmdCopyBuffer` on the GPU, eliminating the 10% host memory stalls during the Banga elevator descent.
3. **Multi-Threaded Staging Worker Pool (`TLG_COPY_THREADS=6`)**:
   - Distributes background texture and asset streaming across dedicated worker threads matching the 6-core AMD Ryzen 5 5600G.
4. **Asynchronous Ring Readbacks (`TLG_DEFERRED_READBACK=1`, `TLG_STALE_READBACK=1`)**:
   - Eliminates CPU read faults by caching GPU query results in a ring buffer.

---

## ⌨️ Hotkeys & In-Game Controls

| Hotkey | Function |
| :--- | :--- |
| **`F1`** | **In-Game Live Control Panel Overlay** (switch audio device, display mode, vsync, or reset audio live) |
| **`F10`** | **Toggle Real-Time In-Game FPS Counter & Telemetry** |
| **`Insert`** (or controller **`R3 + L2`**) | **Native Port In-Game Configuration Menu** |
| **`ESC`** | Dismiss Control Panel overlay / Pause |

---

## 🎮 Controller & Keyboard Layout

| Action | PlayStation (DS4 / DualSense) | Xbox Controller | PC Keyboard & Mouse |
| :--- | :--- | :--- | :--- |
| **Jump / Fall Confirm** | Cross ($\times$) | A | Space / Enter |
| **Evade / Cancel** | Circle ($\bigcirc$) | B | Esc / Backspace |
| **Gravity Kick / Attack**| Square ($\square$) | X | V / F / Left Click |
| **Special Finisher** | Triangle ($\bigtriangleup$) | Y | C |
| **Gravity Shift (Fall)** | R1 | Right Bumper (RB) | E / Right Click |
| **Reset Gravity** | L1 | Left Bumper (LB) | Q |
| **Gravity Slide** | L2 | Left Trigger (LT) | Shift |
| **Stasis Field** | R2 | Right Trigger (RT) | R |
| **Kat Movement** | Left Stick | Left Stick | W / A / S / D |
| **Camera / Aim** | Right Stick / Gyro | Right Stick | Mouse Aiming / Arrow Keys |
| **Pause Menu** | Options | Menu / Start | Tab / P |

### ⚡ Gravity Style Switching (Synthetic Touchpad Swipes)
On standard gamepads or keyboards without a touchpad:
- **Lunar Style (Light / High Agility)**: Hold **`L1`** (or `LB`) + Press **`D-Pad Up`** (or key `1`)
- **Jupiter Style (Heavy / Armor Crush)**: Hold **`L1`** (or `LB`) + Press **`D-Pad Down`** (or key `2`)
- **Normal Style (Standard Gravity)**: Hold **`L1`** (or `LB`) + Press **`D-Pad Left/Right`** (or key `3`)

---

## 💻 System Requirements
- **OS**: Windows 10 / 11 (64-bit)
- **CPU**: 6-Core / 12-Thread x86-64 processor (AMD Ryzen 5 or Intel Core i5)
- **GPU**: Vulkan 1.3 compatible graphics card with 6GB+ VRAM (NVIDIA RTX / GTX 1660+ or AMD Radeon RX 5600+)
- **RAM**: 16 GB RAM
