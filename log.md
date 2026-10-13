# Gravity Rush 2 Native PC Port (`gr2port`) — Engineering Log

Welcome to the central, exhaustive engineering log for the **Gravity Rush 2 (PS4) Native PC Port** project. Every design decision, technical finding, command executed, binary investigation, bug encountered, and code change is cataloged here chronologically.

---

## 1. Project Overview & Objective

* **Target Game:** *Gravity Rush 2* (PlayStation 4)
* **Target Version:** Retail Game Base + Update v1.11 + DLCs (PKG format)
* **Core Philosophy:** Direct native host execution of compiled AMD64 (x86-64) code on PC, paired with a custom Win32 virtual memory layout, an adapted Vulkan GNM/GNMX shader translation layer (leveraging the proven headless architecture of `shadPS4`), and game-specific binary hooks.
* **Inspiration & Blueprint:** 
  * `deadinside28/bloodborne_pc` (`bbport`) / `Mrsuss60/bloodborne_pc_windows_port`
  * `FelipePrado0/gow3_pc` (God of War III Remastered PC Port)
* **Primary Reverse Engineering Toolchain:** **REA (Reverse Engineer Anything)** coupled with **Ghidra 12.1+**.

---

## 2. Host Environment Discovery & Hardware Audit

A system probe was conducted to assess hardware capability, graphics features, and available storage.

### Hardware Specifications
* **Operating System:** Windows 11 (64-bit)
* **Processor (CPU):** AMD Ryzen 5 5600G with Radeon Graphics (6 Cores, 12 Logical Processors)
* **Primary GPU:** **NVIDIA GeForce RTX 4060 Ti** (Driver v32.0.16.1088, ~8-16GB VRAM)
  * *Capabilities:* Full Vulkan 1.3 support, hardware raytracing, NVIDIA DLSS tensor cores, unified FP32/INT32 execution.
* **Secondary GPU:** AMD Radeon(TM) Integrated Graphics (Driver v31.0.21916.2)
* **Storage Environment:** 
  * Primary Drive `C:`: ~111.13 GB free space available.
  * *Storage Strategy:* Dedicated external folder `gr2_packages/` created to isolate large multi-gigabyte PKG dumps from the Git repository.

### Developer Tooling Audit
* **Node.js:** `v24.21.0` (Installed)
* **npm:** `11.19.0` (Installed)
* **Git:** `2.x` located at `C:\Program Files\Git\cmd\git.exe`
* **Python:** `3.11.x` located at `C:\Users\faalv\AppData\Local\Programs\Python\Python311\python.exe`
* **Package Manager:** `winget v1.29.380` (Active and operational)
* **Java:** Legacy JRE 1.8 detected $\rightarrow$ Flagged for upgrade to **Microsoft OpenJDK 21** via `winget` to support Ghidra.

---

## 3. Reverse Engineering Framework Setup: REA

To inspect and decompile the game's executable (`eboot.bin` / `eboot.elf`), REA was cloned, compiled, and registered as a Model Context Protocol (MCP) server.

### Actions Executed
1. **Repository Ingestion:**
   ```powershell
   git clone https://github.com/morluto/rea.git .
   ```
2. **Dependency Resolution:**
   ```powershell
   npm install
   ```
3. **Runtime Compilation:**
   ```powershell
   npm run build:cached
   ```
   * Result: Bundled REA runtime, compiled TypeScript contracts, and packaged reverse-engineering skills.
4. **Global CLI Installation:**
   ```powershell
   npm install --global rea-agents
   ```
   * Verified version: `rea --version` $\rightarrow$ `6.4.0`.
5. **Agent & MCP Registration:**
   ```powershell
   rea setup --client antigravity --yes
   ```
   * Result: Configured MCP bridge at `C:\Users\faalv\.gemini\config\mcp_config.json` with 170 analysis tools and aligned skill version 37.
6. **Functional Verification:**
   * Executed PE resource inspection test against `C:\Windows\System32\notepad.exe`.
   * Verified cryptographic evidence generation, SHA-256 validation, and resource directory traversal.

---

## 4. Architectural Research: The `bbport` & `gow3_pc` Blueprints

Extensive research was performed across the GitHub repositories of `deadinside28/bloodborne_pc`, `Mrsuss60/bloodborne_pc_windows_port`, and `FelipePrado0/gow3_pc` to extract the proven architecture for running PS4 games on PC.

### Key Architectural Discoveries
1. **No CPU Emulation:** Modern PCs and the PS4 both utilize the x86-64 instruction set. Emulators like RPCS3 require heavy PowerPC-to-x86 JIT compilation because the PS3 used the Cell processor. For PS4, the game's compiled ELF code runs directly on the host CPU.
2. **Flat Win32 Memory Mirroring:**
   * On PS4 Orbis OS, user memory is mapped in a flat 64-bit virtual address space.
   * Both `bbport` and `gow3_pc` achieve stability by allocating a large virtual address window (`VirtualAlloc` at `0x10000000000ULL` with `PAGE_EXECUTE_READWRITE`), mirroring the exact memory map expected by the compiled ELF segments.
3. **Vulkan GNM/GNMX Shader Translation:**
   * The PS4 GPU uses AMD GCN bytecode.
   * The projects extract the headless video core from `shadPS4`, translating GNM command buffers and GCN shaders into Vulkan SPIR-V on the fly.
4. **Game-Specific Memory Hooks:**
   * Frame timers in PS4 games are often locked to 30 FPS. Delta-time ($\Delta t$) routines must be reverse-engineered in Ghidra/IDA and dynamically patched in memory to scale physics correctly above 30 FPS.
   * `gow3_pc` implements this via runtime memory patching before jumping into the game's entry point (`main`/`entry`).

---

## 5. Gravity Rush 2 Specific Research & Findings

Investigation into community emulation status (`shadPS4` and `GR2fork`) revealed critical game-specific constraints:

1. **Mandatory Game Update v1.11 Requirement:**
   * The 60 FPS physics patch specifically hooks routines present in **Update v1.11**. Applying high-framerate patches to the base v1.00 executable causes instruction misalignment and immediate crashes.
   * *Status:* User confirmed possession of both Base PKG and Update v1.11 PKG.
2. **Gravity Kick Audio Overlap Bug:**
   * Rapidly executing Kat's gravity kick causes audio cues to stack and loop infinitely on PC sound drivers.
   * *Solution:* Identified need for an audio cue deduplication/cooldown hook in the sound driver stubs.
3. **Graphics & Shadow Stability:**
   * On NVIDIA hardware, setting `readbackLinearImages = false` and synchronizing compute pipelines resolves shadow flickering and cel-shaded rendering seams.

---

## 6. Project Configuration & User Specifications

Through the interactive planning and design interview, all architectural branches were resolved:

| Decision Area | Chosen Strategy | Technical Rationale |
| :--- | :--- | :--- |
| **Motion / Gyro Controls** | **Xbox Gamepad + Mouse Aiming Fallback** (with native DualSense gyro support) | Xbox controllers lack gyro hardware. Kat's gravity shift aiming will capture PC mouse movement and feed it into `ScePadData.motionData` angular velocity vectors. |
| **Framerate & Physics** | **Dynamic $\Delta t$ Timestep Scaling** with selectable caps (60, 120, 144 FPS, Uncapped) | Prevents double-speed physics while enabling full utilization of high-refresh PC monitors and the RTX 4060 Ti GPU. |
| **Resolution & Upscaling** | **Selectable Internal Targets (1080p, 1440p, 4K)** with FSR 3 / DLSS & RCAS sharpening | Allows crisp 4K presentation and upscaling flexibility directly from the in-game menu. |
| **UI & Launcher** | **Hybrid:** Desktop Startup Launcher + In-Game ImGui Overlay (`Insert` key) | Combines pre-launch display settings with real-time in-game performance and graphics adjustments. |
| **Reverse Engineering Scope** | **Comprehensive REA / Ghidra Analysis** | Generates full symbol inventories, camera/FOV offsets, and an exportable modding ledger. |
| **Toolchain Provisioning** | **Automated `winget` Pipeline** | Quiet, non-interactive installation of CMake, Ninja, LLVM/Clang, and OpenJDK 21. |

---

## 7. Workspace & Repository Setup

### Directory Layout Established
```
C:\Users\faalv\Desktop\Personal project\
├── efaz_website\              <-- Unrelated project
└── gravityrush2port\          <-- Master container folder for this project
    ├── gr2_packages\          <-- Helper folder: Raw PKG dumps and asset containers (outside git)
    ├── REA\                   <-- Helper folder: REA reverse-engineering framework and engine (outside git)
    └── gr2port\               <-- Standalone Git Repository (Auto-pushed to GitHub)
        ├── .git\
        ├── .gitignore
        ├── README.md
        └── log.md             <-- This comprehensive engineering log
```

### Git Remote Configuration
* **Remote Origin:** `https://github.com/FairoozAlamMahi/gr2_pcport.git`
* **Default Branch:** `main`
* **Sync Status:** Initialized, linked, and tracking origin.

---

## 8. Next Milestones (Phase 1 & Phase 2 Execution)

1. [ ] Install toolchain dependencies (`winget install Kitware.CMake Ninja-build.Ninja LLVM.LLVM Microsoft.OpenJDK.21`).
2. [ ] Ingest Gravity Rush 2 PKG files into `gr2_packages/`.
3. [ ] Extract base files, apply Update v1.11 deltas, and decrypt `eboot.bin` to `eboot.elf`.
4. [ ] Begin Phase 3 REA binary reverse engineering and dependency audit.
