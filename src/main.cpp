#include "common/types.h"
#include "common/logger.h"
#include "memory/vm.h"
#include "loader/elf_loader.h"
#include "kernel/libkernel.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void PrintBanner() {
    std::cout << R"(
===============================================================
       GRAVITY RUSH 2 — NATIVE PC RUNNER (gr2port)
       Based on shadPS4 headless core & Argon Architecture
===============================================================
    )" << "\n";
}

int main(int argc, char* argv[]) {
    PrintBanner();

    std::string elf_path = "game/eboot.elf";
    if (argc > 1) {
        elf_path = argv[1];
    } else if (!fs::exists(elf_path)) {
        // Fallback candidate search
        std::vector<std::string> candidates = {
            "../gr2_packages/patch_extracted/CUSA04943-patch/eboot.elf",
            "../gr2_packages/merged_game/eboot.elf",
            "../gr2_packages/test_patch_extracted/CUSA04943-patch/eboot.elf"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) {
                elf_path = c;
                break;
            }
        }
    }

    LOG_INFO("INIT", "Target Executable: " + elf_path);
    if (!fs::exists(elf_path)) {
        LOG_FATAL("INIT", "Could not locate eboot.elf at: " + elf_path);
        return 1;
    }

    // 1. Initialize Virtual Memory Manager
    if (!Memory::VirtualMemoryManager::Instance().Initialize()) {
        LOG_FATAL("INIT", "Memory Virtualization failed to initialize!");
        return 1;
    }

    // 2. Initialize Subsystems
    Kernel::Initialize();
    Pad::Initialize();
    Video::Initialize();

    // 3. Load ELF into Guest Arena
    if (!Loader::ElfLoader::Instance().Load(elf_path)) {
        LOG_FATAL("INIT", "Failed to load and map ELF!");
        Memory::VirtualMemoryManager::Instance().Shutdown();
        return 1;
    }

    // 4. Initialize Main Thread TCB in Windows TEB
    void* main_tcb = Loader::ElfLoader::Instance().SetupThreadTCB();
    if (!main_tcb) {
        LOG_FATAL("INIT", "Failed to allocate TCB for main thread!");
        Memory::VirtualMemoryManager::Instance().Shutdown();
        return 1;
    }
    LOG_INFO("INIT", "Main thread TCB initialized at 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(main_tcb)));

    void* entry_ptr = Loader::ElfLoader::Instance().GetEntryPoint();
    LOG_INFO("INIT", "Ready to jump to guest entry point at 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(entry_ptr)));

    LOG_INFO("INIT", "Pre-execution initialization successfully completed.");

    // Note: In interactive launch, execution jumps to entry_ptr:
    // using GuestMain = void (PS4_SYSV_ABI *)();
    // auto guest_main = reinterpret_cast<GuestMain>(entry_ptr);
    // guest_main();

    return 0;
}
