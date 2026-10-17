# Contributing to Gravity Rush 2 PC Port (`gr2port`)

Thank you for your interest in contributing to the **Gravity Rush 2 PC Port**! This project is an open-source, community-driven preservation effort dedicated to bringing *Gravity Rush 2* (`CUSA04943` Update v1.11) natively to modern PC hardware.

---

## 🎯 Architecture & Guiding Philosophy

1. **Direct Host AMD64 Execution (Zero CPU Emulation)**:
   - Modern PCs and the PlayStation 4 share the same AMD x86-64 instruction set.
   - The game's compiled machine code (`eboot.bin` / `eboot.elf`) runs directly on the host CPU.
   - We do not write CPU emulators or JIT dynamic recompilers.
2. **Headless Vulkan 1.3 Graphics Translation**:
   - AMD GCN Sea Islands bytecode and PM4 command buffers are translated into Vulkan 1.3 SPIR-V shaders.
   - All rendering code must conform to Vulkan 1.3 specifications without vendor-specific proprietary lock-in.
3. **Decrypted PRX Module Loading**:
   - Rather than reverse-engineering and stubbing thousands of proprietary runtime routines, we load decrypted native Orbis PRX modules (`libc.prx`, `libSceFios2.prx`, `libSceJobManager.prx`, `libSceNpToolkit.prx`) directly into the flat Win32 1 TiB virtual address space.
4. **Clean-Room & Legal Integrity**:
   - **DO NOT commit or submit pull requests containing copyrighted Sony game binaries (`eboot.bin`, `.pkg`, `.psarc`), proprietary assets, textures, audio files, or official encryption keys.**
   - All reverse engineering must be performed using clean-room analysis via REA, Ghidra, or IDA Pro.

---

## 🛠️ Developer Environment Setup

### Required Toolchains
- **Operating System**: Windows 10/11 x64
- **Build System**: CMake 3.28+ and Ninja 1.12+
- **Compiler**: LLVM / Clang 18+ or Microsoft Visual Studio 2022 (v143 toolset)
- **SDK**: Windows SDK 10.0.22621+
- **Vulkan**: Vulkan SDK 1.3+
- **Python**: Python 3.10+ (for analysis and symbol extraction scripts in `scripts/` and `tools/`)

### Building the Project
```powershell
# Clone the repository with all submodules
git clone --recurse-submodules https://github.com/FairoozAlamMahi/gr2_pcport.git
cd gr2_pcport

# Generate build configuration
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compile native runner and tools
ninja -C build
```

### Compiling the Standalone Native Launcher
```powershell
cd src/launcher
C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /optimize+ /win32icon:icon.ico /out:..\..\GravityRush2.exe GravityRush2.cs
```

---

## 🚀 Active Areas for Contribution

We actively welcome contributions in the following areas:

### 1. Audio Subsystem (OpenAL Soft & NGS2)
- **Multi-Port Audio Balancing**: Fine-tuning concurrent volume mixing across the four PS4 output ports (`Main`, `Bgm`, `Audio3d`, `PadSpk`).
- **NGS2 Synthesis Calibration**: Improving Voice Control matrix tracking in `ngs2` to eliminate audio cue overlaps during rapid gravity kicks.

### 2. Vulkan 3D Graphics & Shaders
- **Shader Pipeline Pre-Caching (`.vkcache`)**: Dumping and consolidating a pre-compiled SPIR-V pipeline cache across all 19 draw stages to eliminate first-time shader compilation stutter for new players.
- **Cel-Shading Outlines**: Perfecting edge-detection threshold parameters in the custom fragment shaders.
- **Ultrawide & High-Resolution Scaling**: Implementing an in-engine viewport hook to support 21:9 and 32:9 aspect ratios without black side bars or HUD stretching.

### 3. Physics & Frame Pacing
- **High-Refresh Dynamic $\Delta t$ Clamping**: Ensuring Kat's gravity shift and slide kicks remain physically stable above 120 FPS.
- **Cutscene Clamp Detection**: Detecting comic-book panel playback states and locking presentation to 30/60 FPS to prevent comic transitions from racing ahead.

### 4. Input & Ergonomics
- **Steam Deck & Linux Compatibility**: Testing and tuning controller mapping and Proton/Wine compatibility profiles.
- **DualSense Haptic Feedback**: Mapping Orbis motor rumble commands to Windows DualSense advanced haptic APIs.

---

## 📝 Pull Request & Commit Guidelines

1. **Descriptive Commit Messages**: Follow Conventional Commits format:
   - `feat(graphics): implement 21:9 ultrawide viewport patch`
   - `fix(audio): resolve 8ch downmixing clipping on OpenAL Soft`
   - `docs: update troubleshooting guide for Intel Arc GPUs`
2. **Document Your Work in `log.md`**: Whenever implementing major subsystems or architectural patches, append an entry to [`log.md`](log.md) explaining your technical findings, addresses modified, and verification results.
3. **Test Before Submitting**: Verify that your changes compile cleanly under Clang and run on real hardware without causing regressions in existing subsystems.

Thank you for helping preserve and elevate *Gravity Rush 2* on PC!
