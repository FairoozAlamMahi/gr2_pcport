#include "common/crash_handler.h"
#include "common/logger.h"
#include "loader/elf_loader.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace Common {

static PVOID s_veh_handle = nullptr;
static std::mutex s_crash_mutex;

LONG WINAPI CrashHandler::ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;

    if (code == 0x406D1388 || code == 0x40010006) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Only catch true fatal hardware exceptions; let Windows/CRT/RPC handle internal status codes
    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_PRIV_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_STACK_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const auto* ctx = pExceptionInfo->ContextRecord;
    uintptr_t rip = ctx->Rip;

    // If fault occurred inside a Windows system module (like ntdll, kernel32), let Windows SEH handle it
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(rip), &hMod)) {
        char mod_name[MAX_PATH] = {};
        GetModuleFileNameA(hMod, mod_name, sizeof(mod_name));
        std::string mod_str = mod_name;
        for (char& c : mod_str) c = static_cast<char>(tolower(c));
        if (mod_str.find("windows") != std::string::npos || 
            mod_str.find("system32") != std::string::npos ||
            mod_str.find("ucrtbase") != std::string::npos) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }



    // VEH Dynamic Hot-Patch Guards:
    // 1. Bypass GDB2 database lookup null dereference when GDB2 table is not loaded (rbx == 0 or r12 == 0)
    uintptr_t eboot_base = Loader::ElfLoader::Instance().GetBaseAddress();
    if ((rip == eboot_base + 0x563802 && ctx->Rbx == 0) ||
        (rip == eboot_base + 0x563864 && ctx->R12 == 0)) {
        pExceptionInfo->ContextRecord->Rax = 0;
        pExceptionInfo->ContextRecord->Rip = eboot_base + 0x563908;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // 2. Bypass null database table iteration loop at 0x4579ea and 0x457cbd (if r14 == 0)
    if ((rip == eboot_base + 0x4579ea || rip == eboot_base + 0x457cbd) && ctx->R14 == 0) {
        pExceptionInfo->ContextRecord->Rip = eboot_base + 0x457d5a;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // 3. Bypass null context dereference at 0xc9250e (if rbx == 0) -> return NULL at 0xc9493e
    if (rip == eboot_base + 0xc9250e && ctx->Rbx == 0) {
        pExceptionInfo->ContextRecord->Rip = eboot_base + 0xc9493e;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // 4. Bypass null virtual call at 0x10d970c (if rax == 0) -> continue to 0x10d971a
    if (rip == eboot_base + 0x10d970c && ctx->Rax == 0) {
        pExceptionInfo->ContextRecord->Rip = eboot_base + 0x10d971a;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // 5. Bypass null string magic check at 0x54c503 (if r13 == 0) -> return false at 0x54c63e
    if (rip == eboot_base + 0x54c503 && ctx->R13 == 0) {
        pExceptionInfo->ContextRecord->Rip = eboot_base + 0x54c63e;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    std::lock_guard<std::mutex> lock(s_crash_mutex);

    auto log_out = [](const std::string& msg) {
        std::cerr << msg;
        std::cout << msg;
        std::cerr.flush();
        std::cout.flush();
    };

    std::stringstream diag;
    diag << "\n===============================================================\n"
         << "         [CRASH DIAGNOSTIC] UNHANDLED GUEST EXCEPTION          \n"
         << "===============================================================\n"
         << "Exception Code:    0x" << std::hex << std::uppercase << code << "\n"
         << "Faulting Address:  0x" << rip << "\n";

    hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(rip), &hMod)) {
        char mod_name[MAX_PATH] = {};
        GetModuleFileNameA(hMod, mod_name, sizeof(mod_name));
        uintptr_t rva = rip - reinterpret_cast<uintptr_t>(hMod);
        diag << "Faulting Module:   " << mod_name << " (RVA: 0x" << std::hex << rva << ")\n";
    }

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR is_write = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR target = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        diag << "Violation Type:    " << (is_write == 1 ? "WRITE" : is_write == 8 ? "EXECUTE" : "READ") 
             << " at 0x" << target << "\n";
    }

    diag << "\n--- CPU Register State (AMD64 / System V) ---\n"
         << "RAX: 0x" << ctx->Rax << "  RBX: 0x" << ctx->Rbx << "  RCX: 0x" << ctx->Rcx << "  RDX: 0x" << ctx->Rdx << "\n"
         << "RSI: 0x" << ctx->Rsi << "  RDI: 0x" << ctx->Rdi << "  RBP: 0x" << ctx->Rbp << "  RSP: 0x" << ctx->Rsp << "\n"
         << "R8:  0x" << ctx->R8  << "  R9:  0x" << ctx->R9  << "  R10: 0x" << ctx->R10 << "  R11: 0x" << ctx->R11 << "\n"
         << "R12: 0x" << ctx->R12 << "  R13: 0x" << ctx->R13 << "  R14: 0x" << ctx->R14 << "  R15: 0x" << ctx->R15 << "\n"
         << "EFLAGS: 0x" << ctx->EFlags << "\n"
         << "\n--- Instruction Bytes at RIP ---\n";

    u8 rip_bytes[16] = {};
    SIZE_T rip_read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(rip), rip_bytes, sizeof(rip_bytes), &rip_read) && rip_read > 0) {
        diag << "Bytes: ";
        for (SIZE_T i = 0; i < rip_read; ++i) {
            diag << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rip_bytes[i]) << " ";
        }
        diag << "\n";
    } else {
        diag << "<Failed to read instruction memory at 0x" << std::hex << rip << ">\n";
    }

    diag << "\n--- Stack Trace (Top 32 Words at RSP) ---\n";
    uintptr_t stack_words[32] = {};
    SIZE_T stack_read = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(ctx->Rsp), stack_words, sizeof(stack_words), &stack_read) && stack_read >= 8) {
        size_t count = stack_read / sizeof(uintptr_t);
        for (size_t i = 0; i < count; ++i) {
            uintptr_t addr = stack_words[i];
            std::stringstream ss;
            HMODULE hFrameMod = nullptr;
            if (addr >= 0x10000000000ULL && addr < 0x10080000000ULL) {
                ss << " [eboot.elf + 0x" << std::hex << (addr - 0x10000000000ULL) << "]";
            } else if (addr >= 0x10080000000ULL && addr < 0x10090000000ULL) {
                ss << " [libc.elf + 0x" << std::hex << (addr - 0x10080000000ULL) << "]";
            } else if (addr >= 0x10090000000ULL && addr < 0x100A0000000ULL) {
                ss << " [libSceFios2.elf + 0x" << std::hex << (addr - 0x10090000000ULL) << "]";
            } else if (addr >= GUEST_ARENA_BASE && addr < (GUEST_ARENA_BASE + GUEST_ARENA_SIZE)) {
                ss << " [Guest Arena + 0x" << std::hex << (addr - GUEST_ARENA_BASE) << "]";
            } else if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                          reinterpret_cast<LPCSTR>(addr), &hFrameMod)) {
                char frame_name[MAX_PATH] = {};
                GetModuleFileNameA(hFrameMod, frame_name, sizeof(frame_name));
                const char* base_name = strrchr(frame_name, '\\');
                ss << " [" << (base_name ? base_name + 1 : frame_name) << " + 0x" << std::hex << (addr - reinterpret_cast<uintptr_t>(hFrameMod)) << "]";
            }
            diag << "  RSP+" << std::setw(2) << std::setfill('0') << std::hex << (i * 8) << ": 0x" << addr << ss.str() << "\n";
        }
    } else {
        diag << "<Failed to read stack memory at 0x" << std::hex << ctx->Rsp << ">\n";
    }

    diag << "===============================================================\n\n";
    log_out(diag.str());

    // Fast exit to prevent WER minidump freezing
    ExitProcess(code);
    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::Install() {
    if (!s_veh_handle) {
        s_veh_handle = AddVectoredExceptionHandler(0, ExceptionHandler); // Low priority (after GPU handler)
        LOG_INFO("KERNEL", "Vectored Crash Diagnostic Handler installed.");
    }
}

} // namespace Common
