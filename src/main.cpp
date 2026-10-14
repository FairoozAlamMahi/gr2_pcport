#include "common/types.h"
#include "common/logger.h"
#include "memory/vm.h"
#include "loader/elf_loader.h"
#include "loader/prx_module.h"
#include "loader/nid_table.h"
#include "kernel/libkernel.h"
#include "kernel/time_hooks.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
#include "audio/audio_out.h"
#include "gpu/gnm_driver.h"
#include "common/crash_handler.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

struct EntryParams {
    int argc{1};
    u32 pad{0};
    const char* argv[33]{"eboot.bin", nullptr};
    u64 entry_addr{0};
};

static PS4_SYSV_ABI void GuestExitHandler(int status) {
    LOG_INFO("GUEST", "Guest code requested process exit with status: " + std::to_string(status));
    ExitProcess(status);
}

static void LaunchGuest(void* entry_point, const EntryParams* params, void* exit_func) {
    LOG_INFO("GUEST", "Jumping to Orbis guest entry point at 0x" + 
             std::to_string(reinterpret_cast<uintptr_t>(entry_point)) + " (System V ABI)...");
    asm volatile(
        "movq %0, %%r11\n\t"
        "movq %1, %%rdi\n\t"
        "movq %2, %%rsi\n\t"
        "andq $-16, %%rsp\n\t"
        "subq $8, %%rsp\n\t"
        "jmp *%%r11\n\t"
        :
        : "r"(entry_point), "r"(params), "r"(exit_func)
        : "r11", "rdi", "rsi", "memory"
    );
}

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

    // Allocate guest TLS slot immediately so it receives slot < 64 (inside direct TEB->TlsSlots)
    DWORD guest_tls_slot = TlsAlloc();
    Loader::ElfLoader::Instance().SetGuestTlsSlot(guest_tls_slot);

    // 0. Install Vectored Crash Diagnostic Handler
    Common::CrashHandler::Install();

    std::string elf_path = "game/eboot.elf";
    bool launch_guest = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--launch" || arg == "-l") {
            launch_guest = true;
        } else if (arg.rfind("--", 0) != 0 && arg.rfind("-", 0) != 0) {
            elf_path = arg;
        }
    }

    if (!fs::exists(elf_path)) {
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

    // 2. Initialize Subsystems & Symbol Resolver
    Kernel::Initialize();
    Pad::Initialize();
    Video::Initialize();
    Audio::Initialize();
    GPU::Initialize();
    Loader::NidTable::Instance().Initialize();

    // 2b. Phase 1: Load Decrypted Native Game PRX Modules & Register Exports
    std::vector<std::pair<std::string, uintptr_t>> prx_candidates = {
        {"../gr2_packages/merged_game/sce_module/libc.elf", 0x10080000000ULL},
        {"../gr2_packages/merged_game/sce_module/libSceFios2.elf", 0x10090000000ULL},
        {"../gr2_packages/merged_game/sce_module/libSceJobManager.elf", 0x100A0000000ULL},
        {"../gr2_packages/merged_game/sce_module/libSceNpToolkit.elf", 0x100B0000000ULL}
    };

    std::vector<std::unique_ptr<Loader::PrxModule>> loaded_prxs;
    for (const auto& [prx_file, base_vaddr] : prx_candidates) {
        if (fs::exists(prx_file)) {
            auto prx = std::make_unique<Loader::PrxModule>();
            if (prx->Load(prx_file, base_vaddr)) {
                loaded_prxs.push_back(std::move(prx));
            }
        }
    }

    // 2c. Phase 2: Relocate and Link PRX PLT Jump Slots across all registered NIDs
    LOG_INFO("INIT", "Performing dynamic two-pass linking across loaded PRX modules...");
    for (auto& prx : loaded_prxs) {
        prx->RelocateAndLink();
    }

    // 3. Load ELF into Guest Arena and link all symbols
    if (!Loader::ElfLoader::Instance().Load(elf_path)) {
        LOG_FATAL("INIT", "Failed to load and map ELF!");
        Memory::VirtualMemoryManager::Instance().Shutdown();
        return 1;
    }

    // 3b. Install Dynamic Delta-Time Detours & Physics Protection Hooks
    Kernel::TimeHooks::Instance().Install(Loader::ElfLoader::Instance().GetBaseAddress());

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

    // 4b. Execute SceProcParam Priority 1 initializers (Master Allocators & Memory Management)
    Loader::ElfLoader::Instance().ExecuteProcParamInitializers();

    if (launch_guest) {
        EntryParams params{};
        params.argc = 1;
        params.argv[0] = "eboot.bin";
        params.entry_addr = reinterpret_cast<u64>(entry_ptr);
        LaunchGuest(entry_ptr, &params, reinterpret_cast<void*>(&GuestExitHandler));
    } else {
        LOG_INFO("INIT", "Dry run / verification completed successfully. Pass '--launch' or '-l' to execute guest code.");
    }

    return 0;
}
