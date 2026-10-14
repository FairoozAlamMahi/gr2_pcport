#include "loader/nid_table.h"
#include "common/logger.h"
#include "kernel/libkernel.h"
#include "kernel/save_data.h"
#include "kernel/net_service.h"
#include "kernel/np_service.h"
#include "kernel/user_service.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
#include "video/font_service.h"
#include "video/image_decoder.h"
#include "video/avplayer_service.h"
#include "kernel/dialog_service.h"
#include "kernel/rtc_service.h"
#include "kernel/http_service.h"
#include "kernel/security_service.h"
#include "audio/audio_out.h"
#include "gpu/gnm_driver.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <atomic>

extern "C" void HostStubLogger(u64 stub_id, uintptr_t caller_rip) {
    static std::atomic<bool> s_logged[8192]{};
    if (stub_id < 8192) {
        if (s_logged[stub_id].exchange(true, std::memory_order_relaxed)) {
            return; // Already logged once
        }
    }
    char buf[512];
    const std::string& name = Loader::NidTable::Instance().GetStubName(stub_id);
    int len = snprintf(buf, sizeof(buf), "[WARN ][STUB] Unhandled guest API called: [%s] from 0x%llX\n",
                       name.c_str(), static_cast<unsigned long long>(caller_rip));
    if (len > 0) {
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(len), &written, nullptr);
    }
}

__attribute__((naked)) static void CommonStubEntry() {
    asm volatile(
        "pushq %rdi\n\t"
        "pushq %rsi\n\t"
        "pushq %rdx\n\t"
        "pushq %rcx\n\t"
        "pushq %r8\n\t"
        "pushq %r9\n\t"
        "pushq %r10\n\t"
        "pushq %r11\n\t"
        "movq %rax, %rcx\n\t"
        "movq 64(%rsp), %rdx\n\t"
        "subq $40, %rsp\n\t"
        "callq HostStubLogger\n\t"
        "addq $40, %rsp\n\t"
        "popq %r11\n\t"
        "popq %r10\n\t"
        "popq %r9\n\t"
        "popq %r8\n\t"
        "popq %rcx\n\t"
        "popq %rdx\n\t"
        "popq %rsi\n\t"
        "popq %rdi\n\t"
        "xorq %rax, %rax\n\t"
        "retq\n\t"
    );
}

namespace Loader {

NidTable& NidTable::Instance() {
    static NidTable table;
    return table;
}

void NidTable::Initialize() {
    m_symbols.clear();
    m_names.clear();
    m_stub_cache.clear();
    m_stub_entries.clear();
    if (!m_stub_arena) {
        m_stub_arena = static_cast<u8*>(VirtualAlloc(nullptr, 131072, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    }
    RegisterHostSubsystems();
}

void NidTable::RegisterSymbol(const std::string& nid_prefix, uintptr_t func_addr, const std::string& name) {
    m_symbols[nid_prefix] = func_addr;
    if (!name.empty()) {
        m_names[nid_prefix] = name;
    }
}

uintptr_t NidTable::Resolve(const std::string& nid_prefix, const std::string& full_symbol) {
    auto it = m_symbols.find(nid_prefix);
    if (it != m_symbols.end()) {
        return it->second;
    }

    auto cache_it = m_stub_cache.find(nid_prefix);
    if (cache_it != m_stub_cache.end()) {
        return cache_it->second;
    }

    if (!m_stub_arena) {
        m_stub_arena = static_cast<u8*>(VirtualAlloc(nullptr, 131072, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    }

    u32 stub_id = static_cast<u32>(m_stub_entries.size());
    std::string desc = full_symbol.empty() ? nid_prefix : full_symbol;
    auto name_it = m_names.find(nid_prefix);
    if (name_it != m_names.end()) {
        desc = name_it->second + " (" + desc + ")";
    }
    m_stub_entries.push_back(desc);

    u8* stub_code = m_stub_arena + (stub_id * 32);
    // mov $stub_id, %eax (B8 <imm32>) - 5 bytes
    stub_code[0] = 0xB8;
    *reinterpret_cast<u32*>(&stub_code[1]) = stub_id;

    // jmp *(%rip) - 6 bytes: FF 25 00 00 00 00
    stub_code[5] = 0xFF;
    stub_code[6] = 0x25;
    stub_code[7] = 0x00;
    stub_code[8] = 0x00;
    stub_code[9] = 0x00;
    stub_code[10] = 0x00;
    *reinterpret_cast<uintptr_t*>(&stub_code[11]) = reinterpret_cast<uintptr_t>(&CommonStubEntry);

    // Padding to 32 bytes
    std::memset(&stub_code[19], 0x90, 13);

    uintptr_t stub_addr = reinterpret_cast<uintptr_t>(stub_code);
    m_stub_cache[nid_prefix] = stub_addr;
    return stub_addr;
}

void NidTable::RegisterHostSubsystems() {
    LOG_INFO("NID", "Registering Host System Subsystems into NID Symbol Registry...");


    // ===== libkernel_web.sprx.json (180 symbols) =====
    RegisterSymbol("f7uOxY9mM1U", reinterpret_cast<uintptr_t>(&Kernel::g_stack_chk_guard), "__stack_chk_guard");
    RegisterSymbol("2sWzhYqFH4E", reinterpret_cast<uintptr_t>(&Kernel::g_stdout_obj), "_Stdout");
    RegisterSymbol("H2e8t5ScQGc", reinterpret_cast<uintptr_t>(&Kernel::__pthread_cxa_finalize), "__cxa_finalize");
    RegisterSymbol("UqDGjXA5yUM", reinterpret_cast<uintptr_t>(&Kernel::munmap), "munmap");
    RegisterSymbol("yOfGg-I1ZII", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockattrInit), "scePthreadRwlockattrInit");
    RegisterSymbol("i2ifZ3fS2fo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockattrDestroy), "scePthreadRwlockattrDestroy");
    RegisterSymbol("lbQKqsERhtE", reinterpret_cast<uintptr_t>(&Kernel::sceDiscMapIsRequestOnHDD), "sceDiscMapIsRequestOnHDD");
    RegisterSymbol("fl1eoDnwQ4s", reinterpret_cast<uintptr_t>(&Kernel::sceDiscMapGetPackageSize), "sceDiscMapGetPackageSize");
    RegisterSymbol("fJgP+wqifno", reinterpret_cast<uintptr_t>(&Kernel::ps4_disc_map_get_location), "sceDiscMapGetLocation");
    RegisterSymbol("6kPnj51T62Y", reinterpret_cast<uintptr_t>(&Kernel::sceVideoOutGetResolutionStatus), "sceVideoOutGetResolutionStatus");
    RegisterSymbol("fZo48un7LK4", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceParamGetInt), "sceSystemServiceParamGetInt");
    RegisterSymbol("rPo6tV8D9bM", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceGetStatus), "sceSystemServiceGetStatus");
    RegisterSymbol("656LMQSrg6U", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceReceiveEvent), "sceSystemServiceReceiveEvent");
    RegisterSymbol("99b82IKXpH4", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentAppParamGetInt), "sceAppContentAppParamGetInt");
    RegisterSymbol("xnd8BJzAxmk", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentGetAddcontInfoList), "sceAppContentGetAddcontInfoList");
    RegisterSymbol("yH17Q6NWtVg", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetEvent), "sceUserServiceGetEvent");
    RegisterSymbol("j3YMu1MVNNo", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceInitialize), "sceUserServiceInitialize");
    RegisterSymbol("bwFjS+bX9mA", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceTerminate), "sceUserServiceTerminate");
    RegisterSymbol("CdWp0oHWGr0", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetInitialUser), "sceUserServiceGetInitialUser");
    RegisterSymbol("fPhymKNvK-A", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetLoginUserIdList), "sceUserServiceGetLoginUserIdList");
    RegisterSymbol("1xxcMiGu2fo", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetUserName), "sceUserServiceGetUserName");
    RegisterSymbol("wuI7c7UNk0A", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceRegisterEventCallback), "sceUserServiceRegisterEventCallback");
    RegisterSymbol("spW--yoLQ9o", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceUnregisterEventCallback), "sceUserServiceUnregisterEventCallback");
    RegisterSymbol("zO9UL3qIINQ", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetCurrentNetworkTick), "sceRtcGetCurrentNetworkTick");
    RegisterSymbol("8w-H19ip48I", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetTick), "sceRtcGetTick");
    RegisterSymbol("ts6GlZOKRrE", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoInitialize), "scePlayGoInitialize");
    RegisterSymbol("M1Gma1ocrGE", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoOpen), "scePlayGoOpen");
    RegisterSymbol("uWIYLFkkwqk", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoGetLocus), "scePlayGoGetLocus");
    RegisterSymbol("-RJWNMK3fC8", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoGetProgress), "scePlayGoGetProgress");
    RegisterSymbol("Uco1I0dlDi8", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoClose), "scePlayGoClose");
    RegisterSymbol("MPe0EeBGM-E", reinterpret_cast<uintptr_t>(&Kernel::scePlayGoTerminate), "scePlayGoTerminate");
    RegisterSymbol("TywrFKCoLGY", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataInitialize3), "sceSaveDataInitialize3");
    RegisterSymbol("uoUpLGNkygk", reinterpret_cast<uintptr_t>(&Dialog::sceCommonDialogInitialize), "sceCommonDialogInitialize");
    RegisterSymbol("Vo5V8KAwCmk", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceHideSplashScreen), "sceSystemServiceHideSplashScreen");
    RegisterSymbol("R9lA82OraNs", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentInitialize), "sceAppContentInitialize");
    RegisterSymbol("Jlv-lYxEnvM", reinterpret_cast<uintptr_t>(&Kernel::sceShareUtilityInitialize), "sceShareUtilityInitialize");
    RegisterSymbol("A2CQ3kgSopQ", reinterpret_cast<uintptr_t>(&Kernel::sceNpSetContentRestriction), "sceNpSetContentRestriction");
    RegisterSymbol("Fjc4-n1+y2g", reinterpret_cast<uintptr_t>(&Kernel::__elf_phdr_match_addr), "__elf_phdr_match_addr");
    RegisterSymbol("9BcDykPmo1I", reinterpret_cast<uintptr_t>(&Kernel::__error), "__error");
    RegisterSymbol("kbw4UHHSYy0", reinterpret_cast<uintptr_t>(&Kernel::__pthread_cxa_finalize), "__pthread_cxa_finalize");
    RegisterSymbol("Ou3iL1abvng", reinterpret_cast<uintptr_t>(&Kernel::__stack_chk_fail), "__stack_chk_fail");
    RegisterSymbol("vNe1w4diLCs", reinterpret_cast<uintptr_t>(&Kernel::__tls_get_addr), "__tls_get_addr");
    RegisterSymbol("6Z83sYWFlA8", reinterpret_cast<uintptr_t>(&Kernel::ps4_exit), "_exit");
    RegisterSymbol("crb5j7mkk1c", reinterpret_cast<uintptr_t>(&Kernel::_is_signal_return), "_is_signal_return");
    RegisterSymbol("DRuBt2pvICk", reinterpret_cast<uintptr_t>(&Kernel::ps4_read), "_read");
    RegisterSymbol("p5EcQeEeJAE", reinterpret_cast<uintptr_t>(&Kernel::_sceKernelRtldSetApplicationHeapAPI), "_sceKernelRtldSetApplicationHeapAPI");
    RegisterSymbol("6xVpy0Fdq+I", reinterpret_cast<uintptr_t>(&Kernel::_sigprocmask), "_sigprocmask");
    RegisterSymbol("FxVZqBAA7ks", reinterpret_cast<uintptr_t>(&Kernel::ps4_write), "_write");
    RegisterSymbol("3e+4Iv7IJ8U", reinterpret_cast<uintptr_t>(&Kernel::ps4_accept), "accept");
    RegisterSymbol("KuOmgKoqCdY", reinterpret_cast<uintptr_t>(&Kernel::ps4_bind), "bind");
    RegisterSymbol("lLMT9vJAck0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClockGettime), "clock_gettime");
    RegisterSymbol("bY-PO6JhzhQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClose), "close");
    RegisterSymbol("k+AXqu2-eBc", reinterpret_cast<uintptr_t>(&Kernel::getpagesize), "getpagesize");
    RegisterSymbol("hHlZQUnlxSM", reinterpret_cast<uintptr_t>(&Kernel::getrusage), "getrusage");
    RegisterSymbol("n88vx3C5nW8", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGettimeofday), "gettimeofday");
    RegisterSymbol("pxnCmagrtao", reinterpret_cast<uintptr_t>(&Kernel::ps4_listen), "listen");
    RegisterSymbol("BPE9s9vQQXo", reinterpret_cast<uintptr_t>(&Kernel::mmap), "mmap");
    RegisterSymbol("tZY4+SZNFhA", reinterpret_cast<uintptr_t>(&Kernel::msync), "msync");
    RegisterSymbol("yS8U2TGCe1A", reinterpret_cast<uintptr_t>(&Kernel::ps4_nanosleep), "nanosleep");
    RegisterSymbol("zHchY8ft5pk", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrDestroy), "pthread_attr_destroy");
    RegisterSymbol("wtkt-teR1so", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrInit), "pthread_attr_init");
    RegisterSymbol("E+tyo3lp5Lw", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetdetachstate), "pthread_attr_setdetachstate");
    RegisterSymbol("2Q0z6rnBrTE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetstacksize), "pthread_attr_setstacksize");
    RegisterSymbol("Jmi+9w9u0E4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCreate), "pthread_create_name_np");
    RegisterSymbol("h9CcP3J0oVM", reinterpret_cast<uintptr_t>(&Kernel::scePthreadJoin), "pthread_join");
    RegisterSymbol("ltCfaGr2JGE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexDestroy), "pthread_mutex_destroy");
    RegisterSymbol("ttHNfU+qDBU", reinterpret_cast<uintptr_t>(&Kernel::pthread_mutex_init), "pthread_mutex_init");
    RegisterSymbol("0TyVk4MSLt0", reinterpret_cast<uintptr_t>(&Kernel::pthread_cond_init), "pthread_cond_init");
    RegisterSymbol("27bAgiJmOh0", reinterpret_cast<uintptr_t>(&Kernel::pthread_cond_timedwait), "pthread_cond_timedwait");
    RegisterSymbol("7H0iTOciTLo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexLock), "pthread_mutex_lock");
    RegisterSymbol("K-jXhbt2gn4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexTrylock), "pthread_mutex_trylock");
    RegisterSymbol("2Z+PpY6CaJg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexUnlock), "pthread_mutex_unlock");
    RegisterSymbol("HF7lK46xzjY", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrDestroy), "pthread_mutexattr_destroy");
    RegisterSymbol("dQHWEsJtoE4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrInit), "pthread_mutexattr_init");
    RegisterSymbol("mDmgMOGVUqg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrSettype), "pthread_mutexattr_settype");
    RegisterSymbol("EotR8a3ASf4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSelf), "pthread_self");
    RegisterSymbol("Ez8xjo9UF4E", reinterpret_cast<uintptr_t>(&Kernel::ps4_recv), "recv");
    RegisterSymbol("NN01qLRhiqU", reinterpret_cast<uintptr_t>(&Kernel::sceKernelRename), "rename");
    RegisterSymbol("VHCS3rCd0PM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAddReadEvent), "sceKernelAddReadEvent");
    RegisterSymbol("57ZK+ODEXWY", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAddTimerEvent), "sceKernelAddTimerEvent");
    RegisterSymbol("4R6-OvI2cEA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAddUserEvent), "sceKernelAddUserEvent");
    RegisterSymbol("R-tyYMpYaxY", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAddWriteEvent), "sceKernelAddWriteEvent");
    RegisterSymbol("rTXw65xmLIA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAllocateDirectMemory), "sceKernelAllocateDirectMemory");
    RegisterSymbol("aNz11fnnzi4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAvailableFlexibleMemorySize), "sceKernelAvailableFlexibleMemorySize");
    RegisterSymbol("PZku4ZrXJqg", reinterpret_cast<uintptr_t>(&Kernel::sceKernelCancelEventFlag), "sceKernelCancelEventFlag");
    RegisterSymbol("7uhBFWRAS60", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClearEventFlag), "sceKernelClearEventFlag");
    RegisterSymbol("QBi7HCK03hw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClockGettime), "sceKernelClockGettime");
    RegisterSymbol("UK2Tl2DWUns", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClose), "sceKernelClose");
    RegisterSymbol("0NTHN1NKONI", reinterpret_cast<uintptr_t>(&Kernel::sceKernelConvertLocaltimeToUtc), "sceKernelConvertLocaltimeToUtc");
    RegisterSymbol("-o5uEDpN+oY", reinterpret_cast<uintptr_t>(&Kernel::sceKernelConvertUtcToLocaltime), "sceKernelConvertUtcToLocaltime");
    RegisterSymbol("D0OdFMjp46I", reinterpret_cast<uintptr_t>(&Kernel::sceKernelCreateEqueue), "sceKernelCreateEqueue");
    RegisterSymbol("BpFoboUJoZU", reinterpret_cast<uintptr_t>(&Kernel::sceKernelCreateEventFlag), "sceKernelCreateEventFlag");
    RegisterSymbol("188x57JYp0g", reinterpret_cast<uintptr_t>(&Kernel::sceKernelCreateSema), "sceKernelCreateSema");
    RegisterSymbol("OMDRKKAZ8I4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDebugRaiseException), "sceKernelDebugRaiseException");
    RegisterSymbol("zE-wXIZjLoM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDebugRaiseException), "sceKernelDebugRaiseExceptionOnReleaseMode");
    RegisterSymbol("jpFjmgAC5AE", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteEqueue), "sceKernelDeleteEqueue");
    RegisterSymbol("8mql9OcQnd4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteEventFlag), "sceKernelDeleteEventFlag");
    RegisterSymbol("JxJ4tfgKlXA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteReadEvent), "sceKernelDeleteReadEvent");
    RegisterSymbol("R1Jvn8bSCW8", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteSema), "sceKernelDeleteSema");
    RegisterSymbol("YWQFUyXIVdU", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteTimerEvent), "sceKernelDeleteTimerEvent");
    RegisterSymbol("LJDwdSNTnDg", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteUserEvent), "sceKernelDeleteUserEvent");
    RegisterSymbol("cBGTk8S92XM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelDeleteWriteEvent), "sceKernelDeleteWriteEvent");
    RegisterSymbol("kBwCPsYX-m4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelFstat), "sceKernelFstat");
    RegisterSymbol("fTx66l5iWIA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelFsync), "sceKernelFsync");
    RegisterSymbol("VW3TVZiM4-E", reinterpret_cast<uintptr_t>(&Kernel::sceKernelFtruncate), "sceKernelFtruncate");
    RegisterSymbol("g0VTBxfJyu0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetCurrentCpu), "sceKernelGetCurrentCpu");
    RegisterSymbol("pO96TwzOm5E", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetDirectMemorySize), "sceKernelGetDirectMemorySize");
    RegisterSymbol("23CPPI1tyBY", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetEventFilter), "sceKernelGetEventFilter");
    RegisterSymbol("mJ7aghmgvfc", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetEventId), "sceKernelGetEventId");
    RegisterSymbol("RpQJJVKTiFM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetModuleInfoForUnwind), "sceKernelGetModuleInfoForUnwind");
    RegisterSymbol("f7KBOafysXo", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetModuleInfoFromAddr), "sceKernelGetModuleInfoFromAddr");
    RegisterSymbol("959qrazPIrg", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcParam), "sceKernelGetProcParam");
    RegisterSymbol("4J2sUJmuHZQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcessTime), "sceKernelGetProcessTime");
    RegisterSymbol("fgxnMeTNUtY", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcessTimeCounter), "sceKernelGetProcessTimeCounter");
    RegisterSymbol("BNowx2l588E", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcessTimeCounterFrequency), "sceKernelGetProcessTimeCounterFrequency");
    RegisterSymbol("1j3S3n-tTW4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetTscFrequency), "sceKernelGetTscFrequency");
    RegisterSymbol("j2AIqSqJP0w", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetdents), "sceKernelGetdents");
    RegisterSymbol("taRWhTJFTgE", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetdirentries), "sceKernelGetdirentries");
    RegisterSymbol("ejekcaNQNq0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGettimeofday), "sceKernelGettimeofday");
    RegisterSymbol("-YTW+qXc3CQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelInternalMemoryGetModuleSegmentInfo), "sceKernelInternalMemoryGetModuleSegmentInfo");
    RegisterSymbol("WslcK1FQcGI", reinterpret_cast<uintptr_t>(&Kernel::sceKernelIsNeoMode), "sceKernelIsNeoMode");
    RegisterSymbol("yDBwVAolDgg", reinterpret_cast<uintptr_t>(&Kernel::sceKernelIsStack), "sceKernelIsStack");
    RegisterSymbol("wzvqT4UqKX8", reinterpret_cast<uintptr_t>(&Kernel::sceKernelLoadStartModule), "sceKernelLoadStartModule");
    RegisterSymbol("oib76F-12fk", reinterpret_cast<uintptr_t>(&Kernel::sceKernelLseek), "sceKernelLseek");
    RegisterSymbol("L-Q3LEjIbgA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMapDirectMemory), "sceKernelMapDirectMemory");
    RegisterSymbol("IWIBBdTHit4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMapFlexibleMemory), "sceKernelMapFlexibleMemory");
    RegisterSymbol("mL8NDH86iQI", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMapNamedFlexibleMemory), "sceKernelMapNamedFlexibleMemory");
    RegisterSymbol("1-LFLmRFxxM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMkdir), "sceKernelMkdir");
    RegisterSymbol("3k6kx-zOOSQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMlock), "sceKernelMlock");
    RegisterSymbol("cQke9UuBQOk", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMunmap), "sceKernelMunmap");
    RegisterSymbol("1G3lF1Gg1k8", reinterpret_cast<uintptr_t>(&Kernel::sceKernelOpen), "sceKernelOpen");
    RegisterSymbol("9lvj5DjHZiA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelPollEventFlag), "sceKernelPollEventFlag");
    RegisterSymbol("12wOHk8ywb0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelPollSema), "sceKernelPollSema");
    RegisterSymbol("+r3rMFwItV4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelPread), "sceKernelPread");
    RegisterSymbol("Wl2o5hOVZdw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelPrintBacktraceWithModuleInfo), "sceKernelPrintBacktraceWithModuleInfo");
    RegisterSymbol("nKWi-N2HBV4", reinterpret_cast<uintptr_t>(&Kernel::sceKernelPwrite), "sceKernelPwrite");
    RegisterSymbol("Cg4srZ6TKbU", reinterpret_cast<uintptr_t>(&Kernel::sceKernelRead), "sceKernelRead");
    RegisterSymbol("-2IRUCO--PM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelReadTsc), "sceKernelReadTsc");
    RegisterSymbol("MBuItvba6z8", reinterpret_cast<uintptr_t>(&Kernel::sceKernelReleaseDirectMemory), "sceKernelReleaseDirectMemory");
    RegisterSymbol("teiItL2boFw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelReleaseFlexibleMemory), "sceKernelReleaseFlexibleMemory");
    RegisterSymbol("52NcYU9+lEo", reinterpret_cast<uintptr_t>(&Kernel::sceKernelRename), "sceKernelRename");
    RegisterSymbol("naInUjYt3so", reinterpret_cast<uintptr_t>(&Kernel::sceKernelRmdir), "sceKernelRmdir");
    RegisterSymbol("IOnSvHzqu6A", reinterpret_cast<uintptr_t>(&Kernel::sceKernelSetEventFlag), "sceKernelSetEventFlag");
    RegisterSymbol("ca7v6Cxulzs", reinterpret_cast<uintptr_t>(&Kernel::sceKernelSetGPO), "sceKernelSetGPO");
    RegisterSymbol("4czppHBiriw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelSignalSema), "sceKernelSignalSema");
    RegisterSymbol("-ZR+hG7aDHw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelSleep), "sceKernelSleep");
    RegisterSymbol("eV9wAD2riIA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelStat), "sceKernelStat");
    RegisterSymbol("QKd0qM58Qes", reinterpret_cast<uintptr_t>(&Kernel::sceKernelStopUnloadModule), "sceKernelStopUnloadModule");
    RegisterSymbol("F6e0kwo4cnk", reinterpret_cast<uintptr_t>(&Kernel::sceKernelTriggerUserEvent), "sceKernelTriggerUserEvent");
    RegisterSymbol("WlyEA-sLDf0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelTruncate), "sceKernelTruncate");
    RegisterSymbol("AUXVxWeJU-A", reinterpret_cast<uintptr_t>(&Kernel::sceKernelUnlink), "sceKernelUnlink");
    RegisterSymbol("1jfXLRVzisc", reinterpret_cast<uintptr_t>(&Kernel::sceKernelUsleep), "sceKernelUsleep");
    RegisterSymbol("Xjoosiw+XPI", reinterpret_cast<uintptr_t>(&Kernel::sceKernelUuidCreate), "sceKernelUuidCreate");
    RegisterSymbol("fzyMKs9kim0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWaitEqueue), "sceKernelWaitEqueue");
    RegisterSymbol("JTvBflhYazQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWaitEventFlag), "sceKernelWaitEventFlag");
    RegisterSymbol("Zxa0VhQVTsk", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWaitSema), "sceKernelWaitSema");
    RegisterSymbol("4wSze92BhLI", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWrite), "sceKernelWrite");
    RegisterSymbol("62KCwEMmzcM", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrDestroy), "scePthreadAttrDestroy");
    RegisterSymbol("x1X76arYMxU", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrGet), "scePthreadAttrGet");
    RegisterSymbol("8+s5BzZjxSg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrGetaffinity), "scePthreadAttrGetaffinity");
    RegisterSymbol("-fA+7ZlGDQs", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrGetstacksize), "scePthreadAttrGetstacksize");
    RegisterSymbol("nsYoNRywwNg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrInit), "scePthreadAttrInit");
    RegisterSymbol("3qxgM4ezETA", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetaffinity), "scePthreadAttrSetaffinity");
    RegisterSymbol("-Wreprtu0Qs", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetdetachstate), "scePthreadAttrSetdetachstate");
    RegisterSymbol("El+cQ20DynU", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetguardsize), "scePthreadAttrSetguardsize");
    RegisterSymbol("eXbUSpEaTsA", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetinheritsched), "scePthreadAttrSetinheritsched");
    RegisterSymbol("DzES9hQF4f4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetschedparam), "scePthreadAttrSetschedparam");
    RegisterSymbol("4+h9EzwKF4I", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetschedpolicy), "scePthreadAttrSetschedpolicy");
    RegisterSymbol("UTXzJbWhhTE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadAttrSetstacksize), "scePthreadAttrSetstacksize");
    RegisterSymbol("JGgj7Uvrl+A", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondBroadcast), "scePthreadCondBroadcast");
    RegisterSymbol("g+PZd2hiacg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondDestroy), "scePthreadCondDestroy");
    RegisterSymbol("2Tb92quprl0", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondInit), "scePthreadCondInit");
    RegisterSymbol("kDh-NfxgMtE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondSignal), "scePthreadCondSignal");
    RegisterSymbol("BmMjYxmew1w", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondTimedwait), "scePthreadCondTimedwait");
    RegisterSymbol("WKAXJ4XBPQ4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondWait), "scePthreadCondWait");
    RegisterSymbol("waPcxYiR3WA", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondattrDestroy), "scePthreadCondattrDestroy");
    RegisterSymbol("m5-2bsNfv7s", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCondattrInit), "scePthreadCondattrInit");
    RegisterSymbol("6UgtwV+0zb4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCreate), "scePthreadCreate");
    RegisterSymbol("4qGrR6eoP9Y", reinterpret_cast<uintptr_t>(&Kernel::scePthreadDetach), "scePthreadDetach");
    RegisterSymbol("3PtV6p3QNX4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadEqual), "scePthreadEqual");
    RegisterSymbol("3kg7rT0NQIs", reinterpret_cast<uintptr_t>(&Kernel::scePthreadExit), "scePthreadExit");
    RegisterSymbol("rcrVFJsQWRY", reinterpret_cast<uintptr_t>(&Kernel::scePthreadGetaffinity), "scePthreadGetaffinity");
    RegisterSymbol("1tKyG7RlMJo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadGetprio), "scePthreadGetprio");
    RegisterSymbol("eoht7mQOCmo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadGetspecific), "scePthreadGetspecific");
    RegisterSymbol("EI-5-jlq2dE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadGetthreadid), "scePthreadGetthreadid");
    RegisterSymbol("onNY9Byn-W8", reinterpret_cast<uintptr_t>(&Kernel::scePthreadJoin), "scePthreadJoin");
    RegisterSymbol("geDaqgH9lTg", reinterpret_cast<uintptr_t>(&Kernel::scePthreadKeyCreate), "scePthreadKeyCreate");
    RegisterSymbol("PrdHuuDekhY", reinterpret_cast<uintptr_t>(&Kernel::scePthreadKeyDelete), "scePthreadKeyDelete");
    RegisterSymbol("2Of0f+3mhhE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexDestroy), "scePthreadMutexDestroy");
    RegisterSymbol("cmo1RIYva9o", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexInit), "scePthreadMutexInit");
    RegisterSymbol("9UK1vLZQft4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexLock), "scePthreadMutexLock");
    RegisterSymbol("upoVrzMHFeE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexTrylock), "scePthreadMutexTrylock");
    RegisterSymbol("tn3VlD0hG60", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexUnlock), "scePthreadMutexUnlock");
    RegisterSymbol("smWEktiyyG0", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrDestroy), "scePthreadMutexattrDestroy");
    RegisterSymbol("F8bUHwAG284", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrInit), "scePthreadMutexattrInit");
    RegisterSymbol("1FGvU0i9saQ", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrSetprotocol), "scePthreadMutexattrSetprotocol");
    RegisterSymbol("iMp8QpE+XO4", reinterpret_cast<uintptr_t>(&Kernel::scePthreadMutexattrSettype), "scePthreadMutexattrSettype");
    RegisterSymbol("14bOACANTBo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadOnce), "scePthreadOnce");
    RegisterSymbol("BB+kb08Tl9A", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockDestroy), "scePthreadRwlockDestroy");
    RegisterSymbol("6ULAa0fq4jA", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockInit), "scePthreadRwlockInit");
    RegisterSymbol("Ox9i0c7L5w0", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockRdlock), "scePthreadRwlockRdlock");
    RegisterSymbol("XD3mDeybCnk", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockTryrdlock), "scePthreadRwlockTryrdlock");
    RegisterSymbol("bIHoZCTomsI", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockTrywrlock), "scePthreadRwlockTrywrlock");
    RegisterSymbol("+L98PIbGttk", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockUnlock), "scePthreadRwlockUnlock");
    RegisterSymbol("mqdNorrB+gI", reinterpret_cast<uintptr_t>(&Kernel::scePthreadRwlockWrlock), "scePthreadRwlockWrlock");
    RegisterSymbol("aI+OeCz8xrQ", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSelf), "scePthreadSelf");
    RegisterSymbol("bt3CTBKmGyI", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSetaffinity), "scePthreadSetaffinity");
    RegisterSymbol("W0Hpm2X0uPE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSetprio), "scePthreadSetprio");
    RegisterSymbol("+BzXYkqYeLE", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSetspecific), "scePthreadSetspecific");
    RegisterSymbol("T72hz6ffq08", reinterpret_cast<uintptr_t>(&Kernel::scePthreadYield), "scePthreadYield");
    RegisterSymbol("6XG4B33N09g", reinterpret_cast<uintptr_t>(&Kernel::scePthreadYield), "sched_yield");
    RegisterSymbol("T8fER+tIGgk", reinterpret_cast<uintptr_t>(&Kernel::ps4_select), "select");
    RegisterSymbol("fZOeZIOEmLw", reinterpret_cast<uintptr_t>(&Kernel::ps4_send), "send");
    RegisterSymbol("TUuiYS2kE8s", reinterpret_cast<uintptr_t>(&Kernel::ps4_shutdown), "shutdown");
    RegisterSymbol("VkTAsrZDcJ0", reinterpret_cast<uintptr_t>(&Kernel::sigfillset), "sigfillset");
    RegisterSymbol("VADc3MNQ3cM", reinterpret_cast<uintptr_t>(&Kernel::ps4_signal), "signal");
    RegisterSymbol("aPcyptbOiZs", reinterpret_cast<uintptr_t>(&Kernel::sigprocmask), "sigprocmask");
    RegisterSymbol("mo0bFmWppIw", reinterpret_cast<uintptr_t>(&Kernel::sigreturn), "sigreturn");
    RegisterSymbol("TU-d9PfIHPM", reinterpret_cast<uintptr_t>(&Kernel::ps4_socket), "socket");

    // Standard Memory Allocator (Host-accelerated)
    RegisterSymbol("cVSk9y8URbc", reinterpret_cast<uintptr_t>(&Kernel::ps4_posix_memalign), "posix_memalign");
    RegisterSymbol("gQX+4GDQjpM", reinterpret_cast<uintptr_t>(&Kernel::ps4_malloc), "malloc");
    RegisterSymbol("tIhsqj0qsFE", reinterpret_cast<uintptr_t>(&Kernel::ps4_free), "free");
    RegisterSymbol("2X5agFjKxMc", reinterpret_cast<uintptr_t>(&Kernel::ps4_calloc), "calloc");
    RegisterSymbol("Y7aJ1uydPMo", reinterpret_cast<uintptr_t>(&Kernel::ps4_realloc), "realloc");
    RegisterSymbol("Ujf3KzMvRmI", reinterpret_cast<uintptr_t>(&Kernel::ps4_memalign), "memalign");

    // ===== custom_video_core.elf.json (154 symbols) =====
    RegisterSymbol("s1--uE9mBFw", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutClose), "sceAudioOutClose");
    RegisterSymbol("JfEPXVxhFqA", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutInit), "sceAudioOutInit");
    RegisterSymbol("ekNvsT22rsY", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOpen), "sceAudioOutOpen");
    RegisterSymbol("QOQtbeDqsT4", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOutput), "sceAudioOutOutput");
    RegisterSymbol("w3PdaSTSwGE", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOutputs), "sceAudioOutOutputs");
    RegisterSymbol("GrQ9s4IrNaQ", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutGetPortState), "sceAudioOutGetPortState");
    RegisterSymbol("YaaDbDwKpFM", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dPortGetQueueLevel), "sceAudio3dPortGetQueueLevel");
    RegisterSymbol("i0VnXM-C9fc", reinterpret_cast<uintptr_t>(&Audio::sceNgs2SystemRender), "sceNgs2SystemRender");
    RegisterSymbol("3OdRkSjOcog", reinterpret_cast<uintptr_t>(&Font::sceFontBindRenderer), "sceFontBindRenderer");
    RegisterSymbol("vzHs3C8lWJk", reinterpret_cast<uintptr_t>(&Font::sceFontCloseFont), "sceFontCloseFont");
    RegisterSymbol("n590hj5Oe-k", reinterpret_cast<uintptr_t>(&Font::sceFontCreateLibraryWithEdition), "sceFontCreateLibraryWithEdition");
    RegisterSymbol("WaSFJoRWXaI", reinterpret_cast<uintptr_t>(&Font::sceFontCreateRendererWithEdition), "sceFontCreateRendererWithEdition");
    RegisterSymbol("FXP359ygujs", reinterpret_cast<uintptr_t>(&Font::sceFontDestroyLibrary), "sceFontDestroyLibrary");
    RegisterSymbol("exAxkyVLt0s", reinterpret_cast<uintptr_t>(&Font::sceFontDestroyRenderer), "sceFontDestroyRenderer");
    RegisterSymbol("imxVx8lm+KM", reinterpret_cast<uintptr_t>(&Font::sceFontGetHorizontalLayout), "sceFontGetHorizontalLayout");
    RegisterSymbol("whrS4oksXc4", reinterpret_cast<uintptr_t>(&Font::sceFontMemoryInit), "sceFontMemoryInit");
    RegisterSymbol("h6hIgxXEiEc", reinterpret_cast<uintptr_t>(&Font::sceFontMemoryTerm), "sceFontMemoryTerm");
    RegisterSymbol("gdUCnU0gHdI", reinterpret_cast<uintptr_t>(&Font::sceFontRenderSurfaceInit), "sceFontRenderSurfaceInit");
    RegisterSymbol("oM+XCzVG3oM", reinterpret_cast<uintptr_t>(&Font::sceFontSelectLibraryFt), "sceFontSelectLibraryFt");
    RegisterSymbol("Xx974EW-QFY", reinterpret_cast<uintptr_t>(&Font::sceFontSelectRendererFt), "sceFontSelectRendererFt");
    RegisterSymbol("6vGCkkQJOcI", reinterpret_cast<uintptr_t>(&Font::sceFontSetupRenderScalePixel), "sceFontSetupRenderScalePixel");
    RegisterSymbol("mz2iTY0MK4A", reinterpret_cast<uintptr_t>(&Font::sceFontSupportExternalFonts), "sceFontSupportExternalFonts");
    RegisterSymbol("1QjhKxrsOB8", reinterpret_cast<uintptr_t>(&Font::sceFontUnbindRenderer), "sceFontUnbindRenderer");
    RegisterSymbol("b0xyllnVY-I", reinterpret_cast<uintptr_t>(&GPU::sceGnmAddEqEvent), "sceGnmAddEqEvent");
    RegisterSymbol("b08AgtPlHPg", reinterpret_cast<uintptr_t>(&GPU::sceGnmAreSubmitsAllowed), "sceGnmAreSubmitsAllowed");
    RegisterSymbol("ihxrbsoSKWc", reinterpret_cast<uintptr_t>(&GPU::sceGnmBeginWorkload), "sceGnmBeginWorkload");
    m_names["5udAm+6boVg"] = "sceGnmCreateWorkloadStream";
    m_names["qpGITzPE+Zc"] = "sceGnmDebugHardwareStatus";
    RegisterSymbol("PVT+fuoS9gU", reinterpret_cast<uintptr_t>(&GPU::sceGnmDeleteEqEvent), "sceGnmDeleteEqEvent");
    m_names["UtObDRQiGbs"] = "sceGnmDestroyWorkloadStream";
    RegisterSymbol("bX5IbRvECXk", reinterpret_cast<uintptr_t>(&GPU::sceGnmDingDong), "sceGnmDingDong");
    RegisterSymbol("byXlqupd8cE", reinterpret_cast<uintptr_t>(&GPU::sceGnmDingDong), "sceGnmDingDongForWorkload");
    m_names["HHo1BAljZO8"] = "sceGnmDisableMipStatsReport";
    RegisterSymbol("0BzLGljcwBo", reinterpret_cast<uintptr_t>(&GPU::sceGnmDispatchDirect), "sceGnmDispatchDirect");
    m_names["Z43vKp5k7r0"] = "sceGnmDispatchIndirect";
    m_names["wED4ZXCFJT0"] = "sceGnmDispatchIndirectOnMec";
    RegisterSymbol("nF6bFRUBRAU", reinterpret_cast<uintptr_t>(&GPU::sceGnmDispatchInitDefaultHardwareState), "sceGnmDispatchInitDefaultHardwareState");
    RegisterSymbol("HlTPoZ-oY7Y", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndex), "sceGnmDrawIndex");
    RegisterSymbol("GGsn7jMTxw4", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexAuto), "sceGnmDrawIndexAuto");
    RegisterSymbol("ED9-Fjr8Ta4", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexIndirect), "sceGnmDrawIndexIndirect");
    RegisterSymbol("jHdPvIzlpKc", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexMultiInstanced), "sceGnmDrawIndexMultiInstanced");
    RegisterSymbol("oYM+YzfCm2Y", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexOffset), "sceGnmDrawIndexOffset");
    RegisterSymbol("4v+otIIdjqg", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndirect), "sceGnmDrawIndirect");
    RegisterSymbol("yb2cRhagD1I", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawInitDefaultHardwareState350), "sceGnmDrawInitDefaultHardwareState350");
    m_names["stDSYW2SBVM"] = "sceGnmDrawOpaqueAuto";
    m_names["TLV4mswiZ4A"] = "sceGnmDriverCaptureInProgress";
    m_names["R6z1xM3pW-w"] = "sceGnmDriverTraceInProgress";
    m_names["d88anrgNoKY"] = "sceGnmDriverTriggerCapture";
    RegisterSymbol("Fa3x75OOLRA", reinterpret_cast<uintptr_t>(&GPU::sceGnmEndWorkload), "sceGnmEndWorkload");
    RegisterSymbol("iBt3Oe00Kvc", reinterpret_cast<uintptr_t>(&GPU::sceGnmFlushGarlic), "sceGnmFlushGarlic");
    m_names["UoYY0DWMC0U"] = "sceGnmGetEqEventType";
    m_names["H7-fgvEutM0"] = "sceGnmGetEqTimeStamp";
    RegisterSymbol("oL4hGI1PMpw", reinterpret_cast<uintptr_t>(&GPU::sceGnmGetGpuBlockStatus), "sceGnmGetGpuBlockStatus");
    m_names["tZCSL5ulnB4"] = "sceGnmGetGpuInfoStatus";
    m_names["iFirFzgYsvw"] = "sceGnmGetLastWaitedAddress";
    m_names["KnldROUkWJY"] = "sceGnmGetNumTcaUnits";
    m_names["fzJdEihTFV4"] = "sceGnmGetProtectionFaultTimeStamp";
    m_names["nEyFbYUloIM"] = "sceGnmGetShaderProgramBaseAddress";
    m_names["k7iGTvDQPLQ"] = "sceGnmGetShaderStatus";
    RegisterSymbol("ln33zjBrfjk", reinterpret_cast<uintptr_t>(&GPU::sceGnmGetTheTessellationFactorRingBufferBaseAddress), "sceGnmGetTheTessellationFactorRingBufferBaseAddress");
    RegisterSymbol("7qZVNgEu+SY", reinterpret_cast<uintptr_t>(&GPU::sceGnmInsertPopMarker), "sceGnmInsertPopMarker");
    m_names["aPIZJTXC+cU"] = "sceGnmInsertPushColorMarker";
    RegisterSymbol("W1Etj-jlW7Y", reinterpret_cast<uintptr_t>(&GPU::sceGnmInsertPushMarker), "sceGnmInsertPushMarker");
    m_names["aj3L-iaFmyk"] = "sceGnmInsertSetColorMarker";
    m_names["jiItzS6+22g"] = "sceGnmInsertSetMarker";
    m_names["URDgJcXhQOs"] = "sceGnmInsertThreadTraceMarker";
    RegisterSymbol("1qXLHIpROPE", reinterpret_cast<uintptr_t>(&GPU::sceGnmInsertWaitFlipDone), "sceGnmInsertWaitFlipDone");
    m_names["jg33rEKLfVs"] = "sceGnmIsUserPaEnabled";
    m_names["26PM5Mzl8zc"] = "sceGnmLogicalCuIndexToPhysicalCuIndex";
    m_names["RU74kek-N0c"] = "sceGnmLogicalCuMaskToPhysicalCuMask";
    RegisterSymbol("29oKvKXzEZo", reinterpret_cast<uintptr_t>(&GPU::sceGnmMapComputeQueue), "sceGnmMapComputeQueue");
    RegisterSymbol("A+uGq+3KFtQ", reinterpret_cast<uintptr_t>(&GPU::sceGnmMapComputeQueueWithPriority), "sceGnmMapComputeQueueWithPriority");
    m_names["gObODli-OH8"] = "sceGnmRequestFlipAndSubmitDone";
    m_names["6YRHhh5mHCs"] = "sceGnmRequestFlipAndSubmitDoneForWorkload";
    m_names["f85orjx7qts"] = "sceGnmRequestMipStatsReportAndReset";
    m_names["MYRtYhojKdA"] = "sceGnmResetVgtControl";
    RegisterSymbol("Kx-h-nWQJ8A", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetCsShaderWithModifier), "sceGnmSetCsShaderWithModifier");
    m_names["X9Omw9dwv5M"] = "sceGnmSetEmbeddedPsShader";
    RegisterSymbol("+AFvOEXrKJk", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetEmbeddedVsShader), "sceGnmSetEmbeddedVsShader");
    m_names["FUHG8sQ3R58"] = "sceGnmSetEsShader";
    m_names["jtkqXpAOY6w"] = "sceGnmSetGsRingSizes";
    m_names["UJwNuMBcUAk"] = "sceGnmSetGsShader";
    m_names["VJNjFtqiF5w"] = "sceGnmSetHsShader";
    m_names["vckdzbQ46SI"] = "sceGnmSetLsShader";
    RegisterSymbol("5uFKckiJYRM", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetPsShader350), "sceGnmSetPsShader350");
    m_names["cFCp0NX8wf0"] = "sceGnmSetVgtControl";
    RegisterSymbol("gAhCn6UiU4Y", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetVsShader), "sceGnmSetVsShader");
    m_names["XiyzNZ9J4nQ"] = "sceGnmSetWaveLimitMultipliers";
    m_names["+xuDhxlWRPg"] = "sceGnmSetupMipStatsReport";
    RegisterSymbol("xbxNatawohc", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitAndFlipCommandBuffers), "sceGnmSubmitAndFlipCommandBuffers");
    RegisterSymbol("Ga6r7H6Y0RI", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitAndFlipCommandBuffers), "sceGnmSubmitAndFlipCommandBuffersForWorkload");
    RegisterSymbol("zwY0YV91TTI", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitCommandBuffers), "sceGnmSubmitCommandBuffers");
    RegisterSymbol("jRcI8VcgTz4", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitCommandBuffers), "sceGnmSubmitCommandBuffersForWorkload");
    RegisterSymbol("yvZ73uQUqrk", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitDone), "sceGnmSubmitDone");
    m_names["ArSg-TGinhk"] = "sceGnmUnmapComputeQueue";
    m_names["nLM2i2+65hA"] = "sceGnmUpdateGsShader";
    m_names["GNlx+y7xPdE"] = "sceGnmUpdateHsShader";
    RegisterSymbol("mLVL7N7BVBg", reinterpret_cast<uintptr_t>(&GPU::sceGnmUpdatePsShader350), "sceGnmUpdatePsShader350");
    m_names["V31V01UiScY"] = "sceGnmUpdateVsShader";
    RegisterSymbol("hvG6GfBMXg8", reinterpret_cast<uintptr_t>(&Http::sceHttpAbortRequest), "sceHttpAbortRequest");
    RegisterSymbol("EY28T2bkN7k", reinterpret_cast<uintptr_t>(&Http::sceHttpAddRequestHeader), "sceHttpAddRequestHeader");
    RegisterSymbol("qgxDBjorUxs", reinterpret_cast<uintptr_t>(&Http::sceHttpCreateConnectionWithURL), "sceHttpCreateConnectionWithURL");
    RegisterSymbol("Aeu5wVKkF9w", reinterpret_cast<uintptr_t>(&Http::sceHttpCreateRequestWithURL), "sceHttpCreateRequestWithURL");
    RegisterSymbol("0gYjPTR-6cY", reinterpret_cast<uintptr_t>(&Http::sceHttpCreateTemplate), "sceHttpCreateTemplate");
    RegisterSymbol("P6A3ytpsiYc", reinterpret_cast<uintptr_t>(&Http::sceHttpDeleteConnection), "sceHttpDeleteConnection");
    RegisterSymbol("qe7oZ+v4PWA", reinterpret_cast<uintptr_t>(&Http::sceHttpDeleteRequest), "sceHttpDeleteRequest");
    RegisterSymbol("4I8vEpuEhZ8", reinterpret_cast<uintptr_t>(&Http::sceHttpDeleteTemplate), "sceHttpDeleteTemplate");
    RegisterSymbol("aCYPMSUIaP8", reinterpret_cast<uintptr_t>(&Http::sceHttpGetAllResponseHeaders), "sceHttpGetAllResponseHeaders");
    RegisterSymbol("0onIrKx9NIE", reinterpret_cast<uintptr_t>(&Http::sceHttpGetLastErrno), "sceHttpGetLastErrno");
    RegisterSymbol("yuO2H2Uvnos", reinterpret_cast<uintptr_t>(&Http::sceHttpGetResponseContentLength), "sceHttpGetResponseContentLength");
    RegisterSymbol("0a2TBNfE3BU", reinterpret_cast<uintptr_t>(&Http::sceHttpGetStatusCode), "sceHttpGetStatusCode");
    RegisterSymbol("A9cVMUtEp4Y", reinterpret_cast<uintptr_t>(&Http::sceHttpInit), "sceHttpInit");
    RegisterSymbol("hPTXo3bICzI", reinterpret_cast<uintptr_t>(&Http::sceHttpParseResponseHeader), "sceHttpParseResponseHeader");
    RegisterSymbol("P5pdoykPYTk", reinterpret_cast<uintptr_t>(&Http::sceHttpReadData), "sceHttpReadData");
    RegisterSymbol("1e2BNwI-XzE", reinterpret_cast<uintptr_t>(&Http::sceHttpSendRequest), "sceHttpSendRequest");
    RegisterSymbol("T-mGo9f3Pu4", reinterpret_cast<uintptr_t>(&Http::sceHttpSetAutoRedirect), "sceHttpSetAutoRedirect");
    RegisterSymbol("0S9tTH0uqTU", reinterpret_cast<uintptr_t>(&Http::sceHttpSetConnectTimeOut), "sceHttpSetConnectTimeOut");
    RegisterSymbol("yigr4V0-HTM", reinterpret_cast<uintptr_t>(&Http::sceHttpSetRecvTimeOut), "sceHttpSetRecvTimeOut");
    RegisterSymbol("K1d1LqZRQHQ", reinterpret_cast<uintptr_t>(&Http::sceHttpSetResolveRetry), "sceHttpSetResolveRetry");
    RegisterSymbol("Tc-hAYDKtQc", reinterpret_cast<uintptr_t>(&Http::sceHttpSetResolveTimeOut), "sceHttpSetResolveTimeOut");
    RegisterSymbol("xegFfZKBVlw", reinterpret_cast<uintptr_t>(&Http::sceHttpSetSendTimeOut), "sceHttpSetSendTimeOut");
    RegisterSymbol("Ik-KpLTlf7Q", reinterpret_cast<uintptr_t>(&Http::sceHttpTerm), "sceHttpTerm");
    RegisterSymbol("JBN6N-EY+3M", reinterpret_cast<uintptr_t>(&Http::sceHttpsGetSslError), "sceHttpsGetSslError");
    RegisterSymbol("htyBOoWeS58", reinterpret_cast<uintptr_t>(&Http::sceHttpsSetSslCallback), "sceHttpsSetSslCallback");
    RegisterSymbol("fIqNvkKIUPQ", reinterpret_cast<uintptr_t>(&Security::sceLibSecureInit), "sceLibSecureInit");
    RegisterSymbol("t4lnn4rB+U0", reinterpret_cast<uintptr_t>(&Security::sceLibSecureDestroy), "sceLibSecureDestroy");
    RegisterSymbol("pT4ViD0o-bM", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyGetContextSize), "sceLibSecureCryptographyGetContextSize");
    RegisterSymbol("hyfPWjJB8pI", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographySetContext), "sceLibSecureCryptographySetContext");
    RegisterSymbol("aEoi0u2FOiQ", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyEncrypt), "sceLibSecureCryptographyEncrypt");
    RegisterSymbol("hMYgMP-Vuno", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyDecrypt), "sceLibSecureCryptographyDecrypt");
    RegisterSymbol("RsiCFJMIGHw", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyDeleteContext), "sceLibSecureCryptographyDeleteContext");
    RegisterSymbol("XKDzFiGAvhU", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyMessagePadding), "sceLibSecureCryptographyMessagePadding");
    RegisterSymbol("NpcegFLFQpg", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyMessagePaddingSize), "sceLibSecureCryptographyMessagePaddingSize");
    RegisterSymbol("iQw3iQPhvUQ", reinterpret_cast<uintptr_t>(&Net::sceNetCtlCheckCallback), "sceNetCtlCheckCallback");
    RegisterSymbol("0cBgduPRR+M", reinterpret_cast<uintptr_t>(&Net::sceNetCtlGetResult), "sceNetCtlGetResult");
    RegisterSymbol("uBPlr0lbuiI", reinterpret_cast<uintptr_t>(&Net::sceNetCtlGetState), "sceNetCtlGetState");
    RegisterSymbol("UJ+Z7Q+4ck0", reinterpret_cast<uintptr_t>(&Net::sceNetCtlRegisterCallback), "sceNetCtlRegisterCallback");
    RegisterSymbol("Rqm2OnZMCz0", reinterpret_cast<uintptr_t>(&Net::sceNetCtlUnregisterCallback), "sceNetCtlUnregisterCallback");
    RegisterSymbol("Nlev7Lg8k3A", reinterpret_cast<uintptr_t>(&Net::sceNetInit), "sceNetInit");
    RegisterSymbol("dgJBaeJnGpo", reinterpret_cast<uintptr_t>(&Net::sceNetPoolCreate), "sceNetPoolCreate");
    RegisterSymbol("K7RlrTkI-mw", reinterpret_cast<uintptr_t>(&Net::sceNetPoolDestroy), "sceNetPoolDestroy");
    RegisterSymbol("cTGkc6-TBlI", reinterpret_cast<uintptr_t>(&Net::sceNetTerm), "sceNetTerm");
    RegisterSymbol("U546k6orxQo", reinterpret_cast<uintptr_t>(&Audio::sceNgs2RackCreateWithAllocator), "sceNgs2RackCreateWithAllocator");
    m_names["lCqD7oycmIM"] = "sceNgs2RackDestroy";
    RegisterSymbol("MwmHz8pAdAo", reinterpret_cast<uintptr_t>(&Audio::sceNgs2RackGetVoiceHandle), "sceNgs2RackGetVoiceHandle");
    RegisterSymbol("mPYgU4oYpuY", reinterpret_cast<uintptr_t>(&Audio::sceNgs2SystemCreateWithAllocator), "sceNgs2SystemCreateWithAllocator");
    m_names["u-WrYDaJA3k"] = "sceNgs2SystemDestroy";
    m_names["i0VnXM-C9fc"] = "sceNgs2SystemRender";
    RegisterSymbol("uu94irFOGpA", reinterpret_cast<uintptr_t>(&Audio::sceNgs2VoiceControl), "sceNgs2VoiceControl");
    RegisterSymbol("-TOuuAQ-buE", reinterpret_cast<uintptr_t>(&Audio::sceNgs2VoiceGetState), "sceNgs2VoiceGetState");
    m_names["u9YKpRRHe-M"] = "sceRazorCaptureImmediate";
    m_names["f33OrruQYbM"] = "sceRazorIsLoaded";
    RegisterSymbol("DOwXL+FQMEY", reinterpret_cast<uintptr_t>(&Http::sceSslGetSerialNumber), "sceSslGetSerialNumber");
    RegisterSymbol("hdpVEUDFW3s", reinterpret_cast<uintptr_t>(&Http::sceSslInit), "sceSslInit");
    RegisterSymbol("0K1yQ6Lv-Yc", reinterpret_cast<uintptr_t>(&Http::sceSslTerm), "sceSslTerm");
    RegisterSymbol("fMP5NHUOaMk", reinterpret_cast<uintptr_t>(&Kernel::sceSysmoduleIsLoaded), "sceSysmoduleIsLoaded");
    RegisterSymbol("g8cM39EUZ6o", reinterpret_cast<uintptr_t>(&Kernel::sceSysmoduleLoadModule), "sceSysmoduleLoadModule");
    RegisterSymbol("eR2bZFAAU0Q", reinterpret_cast<uintptr_t>(&Kernel::sceSysmoduleUnloadModule), "sceSysmoduleUnloadModule");

    // ===== libSceNpMatching2.sprx.json (31 symbols) =====
    m_names["7vjNQ6Z1op0"] = "sceNpMatching2ContextStart";
    m_names["-f6M4caNe8k"] = "sceNpMatching2ContextStop";
    m_names["YfmpW719rMo"] = "sceNpMatching2CreateContext";
    m_names["zCWZmXXN600"] = "sceNpMatching2CreateJoinRoom";
    m_names["Nz-ZE7ur32I"] = "sceNpMatching2DestroyContext";
    m_names["26vWrPAWJfM"] = "sceNpMatching2GetRoomDataExternalList";
    m_names["Jraxifmoet4"] = "sceNpMatching2GetRoomDataInternal";
    m_names["5lhvOqheFBA"] = "sceNpMatching2GetRoomMemberDataInternal";
    m_names["LhCPctIICxQ"] = "sceNpMatching2GetServerId";
    m_names["qeF-q5KDtAc"] = "sceNpMatching2GetUserInfoList";
    m_names["rJNPJqDCpiI"] = "sceNpMatching2GetWorldInfoList";
    RegisterSymbol("10t3e5+JPnU", reinterpret_cast<uintptr_t>(&Np::sceNpMatching2Initialize), "sceNpMatching2Initialize");
    m_names["CSIMDsVjs-g"] = "sceNpMatching2JoinRoom";
    m_names["AUVfU6byg3c"] = "sceNpMatching2KickoutRoomMember";
    m_names["BD6kfx442Do"] = "sceNpMatching2LeaveRoom";
    m_names["fQQfP87I7hs"] = "sceNpMatching2RegisterContextCallback";
    m_names["p+2EnxmaAMM"] = "sceNpMatching2RegisterRoomEventCallback";
    m_names["uBESzz4CQws"] = "sceNpMatching2RegisterRoomMessageCallback";
    m_names["0UMeWRGnZKA"] = "sceNpMatching2RegisterSignalingCallback";
    m_names["VqZX7POg2Mk"] = "sceNpMatching2SearchRoom";
    m_names["opDpl74pi2E"] = "sceNpMatching2SendRoomChatMessage";
    m_names["Iw2h0Jrrb5U"] = "sceNpMatching2SendRoomMessage";
    m_names["+8e7wXLmjds"] = "sceNpMatching2SetDefaultRequestOptParam";
    m_names["q7GK98-nYSE"] = "sceNpMatching2SetRoomDataExternal";
    m_names["S9D8JSYIrjE"] = "sceNpMatching2SetRoomDataInternal";
    m_names["HoqTrkS9c5Q"] = "sceNpMatching2SetRoomMemberDataInternal";
    m_names["meEjIdbjAA0"] = "sceNpMatching2SetUserInfo";
    m_names["twVupeaYYrk"] = "sceNpMatching2SignalingGetConnectionInfo";
    m_names["wUmwXZHaX1w"] = "sceNpMatching2SignalingGetPingInfo";
    RegisterSymbol("Mqp3lJ+sjy4", reinterpret_cast<uintptr_t>(&Np::sceNpMatching2Terminate), "sceNpMatching2Terminate");
    RegisterSymbol("-PoIzr3PEk0", reinterpret_cast<uintptr_t>(&Http::sceSslGetMemoryPoolStats), "sceSslGetMemoryPoolStats");

    // ===== libSceNpManager.sprx.json (25 symbols) =====
    m_names["OzKvTvg3ZYU"] = "sceNpAbortRequest";
    RegisterSymbol("3Zl8BePTh9Y", reinterpret_cast<uintptr_t>(&Np::sceNpCheckCallback), "sceNpCheckCallback");
    RegisterSymbol("JELHf4xPufo", reinterpret_cast<uintptr_t>(&Np::sceNpCheckCallbackForLib), "sceNpCheckCallbackForLib");
    RegisterSymbol("2rsFmlGWleQ", reinterpret_cast<uintptr_t>(&Np::sceNpCheckNpAvailability), "sceNpCheckNpAvailability");
    RegisterSymbol("eiqMCt9UshI", reinterpret_cast<uintptr_t>(&Np::sceNpCreateAsyncRequest), "sceNpCreateAsyncRequest");
    RegisterSymbol("a8R9-75u4iM", reinterpret_cast<uintptr_t>(&Np::sceNpGetAccountId), "sceNpGetAccountId");
    RegisterSymbol("p-o74CnoNzY", reinterpret_cast<uintptr_t>(&Np::sceNpGetNpId), "sceNpGetNpId");
    RegisterSymbol("ilwLM4zOmu4", reinterpret_cast<uintptr_t>(&Np::sceNpGetParentalControlInfo), "sceNpGetParentalControlInfo");
    RegisterSymbol("eQH7nWPcAgc", reinterpret_cast<uintptr_t>(&Np::sceNpGetState), "sceNpGetState");
    m_names["F6E4ycq9Dbg"] = "sceNpGetUserIdByOnlineId";
    m_names["s4UEa5iBJdc"] = "sceNpInGameMessageCreateHandle";
    m_names["+anuSx2avHQ"] = "sceNpInGameMessageDeleteHandle";
    m_names["GFhVUpRmbHE"] = "sceNpInGameMessageInitialize";
    m_names["Vh1bhUG6mSs"] = "sceNpInGameMessagePrepare";
    m_names["ON7Sf5XEMmI"] = "sceNpInGameMessageSendData";
    m_names["bMG3cVmUmuk"] = "sceNpInGameMessageTerminate";
    RegisterSymbol("uqcPJLWL08M", reinterpret_cast<uintptr_t>(&Np::sceNpPollAsync), "sceNpPollAsync");
    RegisterSymbol("hw5KNqAAels", reinterpret_cast<uintptr_t>(&Np::sceNpRegisterNpReachabilityStateCallback), "sceNpRegisterNpReachabilityStateCallback");
    RegisterSymbol("0c7HbXRKUt4", reinterpret_cast<uintptr_t>(&Np::sceNpRegisterStateCallbackForToolkit), "sceNpRegisterStateCallbackForToolkit");
    m_names["A2CQ3kgSopQ"] = "sceNpSetContentRestriction";
    m_names["Ec63y59l9tw"] = "sceNpSetNpTitleId";
    m_names["cRILAEvn+9M"] = "sceNpUnregisterNpReachabilityStateCallback";
    m_names["xViqJdDgKl0"] = "sceNpUnregisterPlusEventCallback";
    RegisterSymbol("YIvqqvJyjEc", reinterpret_cast<uintptr_t>(&Np::sceNpUnregisterStateCallbackForToolkit), "sceNpUnregisterStateCallbackForToolkit");
    m_names["jyi5p9XWUSs"] = "sceNpWaitAsync";

    // ===== libSceNpWebApi.sprx.json (23 symbols) =====
    m_names["JzhYTP2fG18"] = "sceNpWebApiAbortRequest";
    m_names["x1Y7yiYSk7c"] = "sceNpWebApiCreateContext";
    m_names["79M-JqvvGo0"] = "sceNpWebApiCreateHandle";
    m_names["y5Ta5JCzQHY"] = "sceNpWebApiCreatePushEventFilter";
    m_names["rdgs5Z1MyFw"] = "sceNpWebApiCreateRequest";
    m_names["sIFx734+xys"] = "sceNpWebApiCreateServicePushEventFilter";
    m_names["XUjdsSTTZ3U"] = "sceNpWebApiDeleteContext";
    m_names["5Mn7TYwpl30"] = "sceNpWebApiDeleteHandle";
    m_names["zE+R6Rcx3W0"] = "sceNpWebApiDeletePushEventFilter";
    m_names["noQgleu+KLE"] = "sceNpWebApiDeleteRequest";
    m_names["PfQ+f6ws764"] = "sceNpWebApiDeleteServicePushEventFilter";
    m_names["VwJ5L0Higg0"] = "sceNpWebApiGetHttpResponseHeaderValue";
    m_names["743ZzEBzlV8"] = "sceNpWebApiGetHttpResponseHeaderValueLength";
    m_names["G3AnLNdRBjE"] = "sceNpWebApiInitialize";
    m_names["CQtPRSF6Ds8"] = "sceNpWebApiReadData";
    m_names["PfSTDCgNMgc"] = "sceNpWebApiRegisterPushEventCallback";
    m_names["kJQJE0uKm5w"] = "sceNpWebApiRegisterServicePushEventCallback";
    m_names["KjNeZ-29ysQ"] = "sceNpWebApiSendRequest2";
    m_names["asz3TtIqGF8"] = "sceNpWebApiTerminate";
    m_names["qK4o2656W4w"] = "sceNpWebApiUnregisterPushEventCallback";
    m_names["2edrkr0c-wg"] = "sceNpWebApiUnregisterServicePushEventCallback";
    m_names["or0e885BlXo"] = "sceNpWebApiUtilityParseNpId";
    m_names["fZo48un7LK4"] = "sceSystemServiceParamGetInt";

    // ===== libSceJson2.sprx.json (20 symbols) =====
    RegisterSymbol("i-XwZjw0OOY", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json12MemAllocator11notifyErrorEimPv), "_ZN3sce4Json12MemAllocator11notifyErrorEimPv");
    RegisterSymbol("-hJRce8wn1U", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json12MemAllocatorC2Ev), "_ZN3sce4Json12MemAllocatorC2Ev");
    RegisterSymbol("OcAgPxcq5Vk", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json12MemAllocatorD2Ev), "_ZN3sce4Json12MemAllocatorD2Ev");
    RegisterSymbol("zQtLRTqceMY", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5Array9push_backERKNS0_5ValueE), "_ZN3sce4Json5Array9push_backERKNS0_5ValueE");
    RegisterSymbol("JP-PtKMiI1E", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ArrayC1Ev), "_ZN3sce4Json5ArrayC1Ev");
    RegisterSymbol("HJ8GpRT1aiw", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ArrayD1Ev), "_ZN3sce4Json5ArrayD1Ev");
    RegisterSymbol("R7FDWtcN6f8", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5Value9serializeERNS0_6StringE), "_ZN3sce4Json5Value9serializeERNS0_6StringE");
    RegisterSymbol("iZeYfOxtMRg", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1ERKNS0_5ArrayE), "_ZN3sce4Json5ValueC1ERKNS0_5ArrayE");
    RegisterSymbol("3xUXnmUkXfo", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1ERKNS0_6ObjectE), "_ZN3sce4Json5ValueC1ERKNS0_6ObjectE");
    RegisterSymbol("sZIoMRGO+jk", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1ERKNS0_6StringE), "_ZN3sce4Json5ValueC1ERKNS0_6StringE");
    RegisterSymbol("a+W7HHlwpBs", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6ObjectC1ERKS1_), "_ZN3sce4Json6ObjectC1ERKS1_");
    RegisterSymbol("OJPTonqdg0I", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6ObjectC1Ev), "_ZN3sce4Json6ObjectC1Ev");
    RegisterSymbol("5JmzZt8twAo", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6ObjectD1Ev), "_ZN3sce4Json6ObjectD1Ev");
    RegisterSymbol("urOpESTBZmo", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6ObjectaSERKS1_), "_ZN3sce4Json6ObjectaSERKS1_");
    RegisterSymbol("ERuf9y0DY84", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6ObjectixERKNS0_6StringE), "_ZN3sce4Json6ObjectixERKNS0_6StringE");
    RegisterSymbol("9KUZFjI1IxA", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6StringC1EPKc), "_ZN3sce4Json6StringC1EPKc");
    RegisterSymbol("qSmqLXXCPas", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6StringC1Ev), "_ZN3sce4Json6StringC1Ev");
    RegisterSymbol("cG1VE2HMl6c", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json6StringD1Ev), "_ZN3sce4Json6StringD1Ev");
    RegisterSymbol("rQGJeNjOuUk", reinterpret_cast<uintptr_t>(&Http::_ZNK3sce4Json5Array4sizeEv), "_ZNK3sce4Json5Array4sizeEv");
    RegisterSymbol("L1KAkYWml-M", reinterpret_cast<uintptr_t>(&Http::_ZNK3sce4Json6String5c_strEv), "_ZNK3sce4Json6String5c_strEv");

    // ===== libSceHttp.sprx.json (19 symbols) =====
    RegisterSymbol("mSQCxzWTwVI", reinterpret_cast<uintptr_t>(&Http::sceHttpsDisableOption), "sceHttpsDisableOption");
    m_names["OXXX4mUk3uk"] = "sceNetConnect";
    RegisterSymbol("Z4wwCFiBELQ", reinterpret_cast<uintptr_t>(&Net::sceNetCtlTerm), "sceNetCtlTerm");
    RegisterSymbol("w21YgGGNtBk", reinterpret_cast<uintptr_t>(&Net::sceNetEpollAbort), "sceNetEpollAbort");
    RegisterSymbol("ZVw46bsasAk", reinterpret_cast<uintptr_t>(&Net::sceNetEpollControl), "sceNetEpollControl");
    RegisterSymbol("SF47kB2MNTo", reinterpret_cast<uintptr_t>(&Net::sceNetEpollCreate), "sceNetEpollCreate");
    RegisterSymbol("Inp1lfL+Jdw", reinterpret_cast<uintptr_t>(&Net::sceNetEpollDestroy), "sceNetEpollDestroy");
    RegisterSymbol("drjIbDbA7UQ", reinterpret_cast<uintptr_t>(&Net::sceNetEpollWait), "sceNetEpollWait");
    m_names["iWQWrwiSt8A"] = "sceNetHtons";
    m_names["9vA2aW+CHuA"] = "sceNetInetNtop";
    m_names["8Kcp5d-q1Uo"] = "sceNetInetPton";
    m_names["Rbvt+5Y2iEw"] = "sceNetNtohs";
    m_names["9wO9XrMsNhc"] = "sceNetRecv";
    m_names["AzqoBha7js4"] = "sceNetResolverAbort";
    m_names["J5i3hiLJMPk"] = "sceNetResolverGetError";
    m_names["beRjXBn-z+o"] = "sceNetSend";
    m_names["Q4qBuN-c0ZM"] = "sceNetSocket";
    m_names["45ggEzakPJQ"] = "sceNetSocketClose";
    RegisterSymbol("07O525HgICs", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddSeconds), "sceRtcTickAddSeconds");

    // ===== libSceNpScoreRanking.sprx.json (19 symbols) =====
    m_names["1i7kmKbX6hk"] = "sceNpScoreAbortRequest";
    m_names["KnNA1TEgtBI"] = "sceNpScoreCreateNpTitleCtx";
    m_names["gW8qyjYrUbk"] = "sceNpScoreCreateRequest";
    m_names["G0pE+RNCwfk"] = "sceNpScoreDeleteNpTitleCtx";
    m_names["dK8-SgYf6r4"] = "sceNpScoreDeleteRequest";
    m_names["LoVMVrijVOk"] = "sceNpScoreGetBoardInfo";
    m_names["8kuIzUw6utQ"] = "sceNpScoreGetFriendsRanking";
    m_names["7SuMUlN7Q6I"] = "sceNpScoreGetFriendsRankingAsync";
    m_names["zKoVok6FFEI"] = "sceNpScoreGetGameData";
    m_names["JjOFRVPdQWc"] = "sceNpScoreGetGameDataAsync";
    m_names["9mZEgoiEq6Y"] = "sceNpScoreGetRankingByNpId";
    m_names["Rd27dqUFZV8"] = "sceNpScoreGetRankingByNpIdAsync";
    m_names["KBHxDjyk-jA"] = "sceNpScoreGetRankingByRange";
    m_names["rShmqXHwoQE"] = "sceNpScoreGetRankingByRangeAsync";
    m_names["m1DfNRstkSQ"] = "sceNpScorePollAsync";
    m_names["bcoVwcBjQ9E"] = "sceNpScoreRecordGameData";
    m_names["1gL5PwYzrrw"] = "sceNpScoreRecordGameDataAsync";
    m_names["zT0XBtgtOSI"] = "sceNpScoreRecordScore";
    m_names["ANJssPz3mY0"] = "sceNpScoreRecordScoreAsync";

    // ===== libSceNpUtility.sprx.json (17 symbols) =====
    m_names["BYIZGKm6bO4"] = "sceNpBandwidthTestGetStatus";
    m_names["jktww3yJXnc"] = "sceNpBandwidthTestInitStart";
    m_names["pLr1fEQS1z8"] = "sceNpBandwidthTestShutdown";
    m_names["iQr9UxPHUFs"] = "sceNpLookupCreateRequest";
    m_names["8533Q+LU7EQ"] = "sceNpLookupCreateTitleCtx";
    m_names["wLaxchvEEnk"] = "sceNpLookupDeleteRequest";
    m_names["mtqDK9zkoIE"] = "sceNpLookupDeleteTitleCtx";
    m_names["T6tnM1Uti4g"] = "sceNpLookupNpId";
    m_names["rAOOqDAxBIk"] = "sceNpWordFilterAbortRequest";
    m_names["1dMndqL-QgE"] = "sceNpWordFilterCensorComment";
    m_names["IEB+vgVoQbw"] = "sceNpWordFilterCreateAsyncRequest";
    m_names["iCq5xW5KQW4"] = "sceNpWordFilterCreateRequest";
    m_names["r9BgI0PfJZg"] = "sceNpWordFilterCreateTitleCtx";
    m_names["PYFS1H70bDs"] = "sceNpWordFilterDeleteRequest";
    m_names["t0P5z5yuFPA"] = "sceNpWordFilterDeleteTitleCtx";
    m_names["ur5SShyG0dk"] = "sceNpWordFilterPollAsync";
    m_names["Jj4mkpFO2gE"] = "sceNpWordFilterSanitizeComment";

    // ===== libSceNpTus.sprx.json (17 symbols) =====
    m_names["-SUR+UoLS6c"] = "sceNpTssGetData";
    m_names["cRVmNrJDbG8"] = "sceNpTusAddAndGetVariable";
    m_names["ukr6FBSrkJw"] = "sceNpTusAddAndGetVariableVUser";
    m_names["BIkMmUfNKWM"] = "sceNpTusCreateNpTitleCtx";
    m_names["3bh2aBvvmvM"] = "sceNpTusCreateRequest";
    m_names["0DT5bP6YzBo"] = "sceNpTusDeleteMultiSlotData";
    m_names["xutwCvsydkk"] = "sceNpTusDeleteMultiSlotDataVUser";
    m_names["H3uq7x0sZOI"] = "sceNpTusDeleteNpTitleCtx";
    m_names["CcIH40dYS88"] = "sceNpTusDeleteRequest";
    m_names["XOzszO4ONWU"] = "sceNpTusGetData";
    m_names["GQHCksS7aLs"] = "sceNpTusGetDataVUser";
    m_names["F+eQlfcka98"] = "sceNpTusGetMultiSlotVariable";
    m_names["uFxVYJEkcmc"] = "sceNpTusGetMultiSlotVariableVUser";
    m_names["4NrufkNCkiE"] = "sceNpTusSetData";
    m_names["+RhzSuuXwxo"] = "sceNpTusSetDataVUser";
    m_names["c6aYoa47YgI"] = "sceNpTusSetMultiSlotVariable";
    m_names["1Cz0hTJFyh4"] = "sceNpTusSetMultiSlotVariableVUser";

    // ===== libSceAvPlayer.sprx.json (14 symbols) =====
    RegisterSymbol("KMcEa+rHsIo", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerAddSource), "sceAvPlayerAddSource");
    RegisterSymbol("NkJwDzKmIlw", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerClose), "sceAvPlayerClose");
    RegisterSymbol("ODJK2sn9w4A", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerEnableStream), "sceAvPlayerEnableStream");
    RegisterSymbol("d8FcbzfAdQw", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerGetStreamInfo), "sceAvPlayerGetStreamInfo");
    RegisterSymbol("JdksQu8pNdQ", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerGetVideoDataEx), "sceAvPlayerGetVideoDataEx");
    RegisterSymbol("aS66RI0gGgo", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerInit), "sceAvPlayerInit");
    RegisterSymbol("UbQoYawOsfY", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerIsActive), "sceAvPlayerIsActive");
    RegisterSymbol("9y5v+fGN4Wk", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerPause), "sceAvPlayerPause");
    RegisterSymbol("HD1YKVU26-M", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerPostInit), "sceAvPlayerPostInit");
    RegisterSymbol("w5moABNwnRY", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerResume), "sceAvPlayerResume");
    RegisterSymbol("OVths0xGfho", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerSetLooping), "sceAvPlayerSetLooping");
    RegisterSymbol("ET4Gr-Uu07s", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStart), "sceAvPlayerStart");
    RegisterSymbol("ZC17w3vB5Lo", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStop), "sceAvPlayerStop");
    RegisterSymbol("hdTyRzCXQeQ", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStreamCount), "sceAvPlayerStreamCount");

    // ===== libSceNpTrophy.sprx.json (13 symbols) =====
    m_names["cqGkYAN-gRw"] = "sceNpTrophyCaptureScreenshot";
    RegisterSymbol("XbkjbobZlCY", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyCreateContext), "sceNpTrophyCreateContext");
    RegisterSymbol("q7U6tEAQf7c", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyCreateHandle), "sceNpTrophyCreateHandle");
    RegisterSymbol("E1Wrwd07Lr8", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyDestroyContext), "sceNpTrophyDestroyContext");
    RegisterSymbol("GNcF4oidY0Y", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyDestroyHandle), "sceNpTrophyDestroyHandle");
    m_names["HLwz1fRIycA"] = "sceNpTrophyGetGameIcon";
    m_names["YYP3f2W09og"] = "sceNpTrophyGetGameInfo";
    m_names["w4uMPmErD4I"] = "sceNpTrophyGetGroupIcon";
    m_names["wTUwGfspKic"] = "sceNpTrophyGetGroupInfo";
    m_names["eBL+l6HG9xk"] = "sceNpTrophyGetTrophyIcon";
    m_names["qqUVGDgQBm0"] = "sceNpTrophyGetTrophyInfo";
    RegisterSymbol("TJCAxto9SEU", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyRegisterContext), "sceNpTrophyRegisterContext");
    RegisterSymbol("28xmRUFao68", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyUnlockTrophy), "sceNpTrophyUnlockTrophy");
    RegisterSymbol("LHuSmO3SLd8", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyGetTrophyUnlockState), "sceNpTrophyGetTrophyUnlockState");

    // ===== libSceJson.sprx.json (12 symbols) =====
    RegisterSymbol("Cxwy7wHq4J0", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE), "_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE");
    RegisterSymbol("PR5k1penBLM", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json11Initializer9terminateEv), "_ZN3sce4Json11Initializer9terminateEv");
    RegisterSymbol("cK6bYHf-Q5E", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json11InitializerC1Ev), "_ZN3sce4Json11InitializerC1Ev");
    RegisterSymbol("RujUxbr3haM", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json11InitializerD1Ev), "_ZN3sce4Json11InitializerD1Ev");
    RegisterSymbol("XL8+BUqjB1w", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5Value3setERKS1_), "_ZN3sce4Json5Value3setERKS1_");
    RegisterSymbol("FIjXN2TkuTs", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5Value5clearEv), "_ZN3sce4Json5Value5clearEv");
    RegisterSymbol("sOmU4vnx3s0", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1Ed), "_ZN3sce4Json5ValueC1Ed");
    RegisterSymbol("0lLK8+kDqmE", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1El), "_ZN3sce4Json5ValueC1El");
    RegisterSymbol("x4AUdbhpRB0", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1Em), "_ZN3sce4Json5ValueC1Em");
    RegisterSymbol("qBMjqyBn3OM", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueC1Ev), "_ZN3sce4Json5ValueC1Ev");
    RegisterSymbol("WTtYf+cNnXI", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueD1Ev), "_ZN3sce4Json5ValueD1Ev");
    RegisterSymbol("4zrm6VrgIAw", reinterpret_cast<uintptr_t>(&Http::_ZN3sce4Json5ValueaSERKS1_), "_ZN3sce4Json5ValueaSERKS1_");

    // ===== libSceAudio3d.sprx.json (12 symbols) =====
    RegisterSymbol("Im+jOoa5WAI", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dGetDefaultOpenParameters), "sceAudio3dGetDefaultOpenParameters");
    RegisterSymbol("UmCvjSmuZIw", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dInitialize), "sceAudio3dInitialize");
    m_names["jO2tec4dJ2M"] = "sceAudio3dObjectReserve";
    m_names["4uyHN9q4ZeU"] = "sceAudio3dObjectSetAttributes";
    m_names["1HXxo-+1qCw"] = "sceAudio3dObjectUnreserve";
    m_names["lw0qrdSjZt8"] = "sceAudio3dPortAdvance";
    m_names["OyVqOeVNtSk"] = "sceAudio3dPortClose";
    m_names["YaaDbDwKpFM"] = "sceAudio3dPortGetQueueLevel";
    RegisterSymbol("XeDDK0xJWQA", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dPortOpen), "sceAudio3dPortOpen");
    m_names["VEVhZ9qd4ZY"] = "sceAudio3dPortPush";
    RegisterSymbol("Yq9bfUQ0uJg", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dPortSetAttribute), "sceAudio3dPortSetAttribute");
    m_names["WW1TS2iz5yc"] = "sceAudio3dTerminate";

    // ===== libSceSaveData.sprx.json (9 symbols) =====
    RegisterSymbol("S1GkePI17zQ", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDelete), "sceSaveDataDelete");
    RegisterSymbol("dyIhnXq-0SM", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDirNameSearch), "sceSaveDataDirNameSearch");
    RegisterSymbol("TywrFKCoLGY", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataInitialize3), "sceSaveDataInitialize3");
    RegisterSymbol("0z45PIH+SNI", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataMount2), "sceSaveDataMount2");
    RegisterSymbol("c88Yy54Mx0w", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataSaveIcon), "sceSaveDataSaveIcon");
    RegisterSymbol("85zul--eGXs", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataSetParam), "sceSaveDataSetParam");
    RegisterSymbol("yKDy8S5yLA0", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataTerminate), "sceSaveDataTerminate");
    RegisterSymbol("WAzWTZm1H+I", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataTransferringMount), "sceSaveDataTransferringMount");
    RegisterSymbol("BMR4F-Uek3E", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataUmount), "sceSaveDataUmount");

    // ===== libSceContentSearch.sprx.json (9 symbols) =====
    m_names["-YbpaF0XS-I"] = "sceContentSearchCloseMetadata";
    m_names["ruNe-FgCzO8"] = "sceContentSearchGetMetadataValue";
    m_names["FRT4EYtZU1Y"] = "sceContentSearchGetMyApplicationIndex";
    m_names["o-RBPV0qr8c"] = "sceContentSearchGetNumOfContent";
    m_names["dPj4ZtRcIWk"] = "sceContentSearchInit";
    m_names["bjAlYWwRTJA"] = "sceContentSearchOpenMetadataByContentId";
    m_names["TEW3IKxYfXc"] = "sceContentSearchSearchContent";
    m_names["1xSZodB2geA"] = "sceContentSearchTerm";
    RegisterSymbol("CdWp0oHWGr0", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetInitialUser), "sceUserServiceGetInitialUser");

    // ===== libSceContentExport.sprx.json (9 symbols) =====
    m_names["tb3cZTCl8Ps"] = "sceContentExportFinish";
    m_names["AOWqIYsgVHs"] = "sceContentExportFromData";
    m_names["uZTQHI50WpY"] = "sceContentExportFromDataWithThumbnail";
    m_names["FzEWeYnAFlI"] = "sceContentExportInit";
    m_names["FCygF4Ec4so"] = "sceContentExportStart";
    m_names["+KDWny9Y-6k"] = "sceContentExportTerm";
    RegisterSymbol("fPhymKNvK-A", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetLoginUserIdList), "sceUserServiceGetLoginUserIdList");
    RegisterSymbol("j3YMu1MVNNo", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceInitialize), "sceUserServiceInitialize");
    RegisterSymbol("bwFjS+bX9mA", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceTerminate), "sceUserServiceTerminate");

    // ===== libSceSaveDataDialog.sprx.json (8 symbols) =====
    RegisterSymbol("fH46Lag88XY", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogClose), "sceSaveDataDialogClose");
    RegisterSymbol("yEiJ-qqr6Cg", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogGetResult), "sceSaveDataDialogGetResult");
    RegisterSymbol("ERKzksauAJA", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogGetStatus), "sceSaveDataDialogGetStatus");
    RegisterSymbol("s9e3+YpRnzw", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogInitialize), "sceSaveDataDialogInitialize");
    RegisterSymbol("4tPhsP6FpDI", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogOpen), "sceSaveDataDialogOpen");
    RegisterSymbol("hay1CfTmLyA", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogProgressBarSetValue), "sceSaveDataDialogProgressBarSetValue");
    RegisterSymbol("YuH2FA7azqQ", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogTerminate), "sceSaveDataDialogTerminate");
    RegisterSymbol("KK3Bdg1RWK0", reinterpret_cast<uintptr_t>(&SaveData::sceSaveDataDialogUpdateStatus), "sceSaveDataDialogUpdateStatus");

    // ===== libSceNet.sprx.json (8 symbols) =====
    m_names["PIWqhn9oSxc"] = "sceNetAccept";
    m_names["bErx49PgxyY"] = "sceNetBind";
    RegisterSymbol("6Oc0bLsIYe0", reinterpret_cast<uintptr_t>(&Net::sceNetGetMacAddress), "sceNetGetMacAddress");
    m_names["TCkRD0DWNLg"] = "sceNetGetpeername";
    m_names["hoOAofhhRvE"] = "sceNetGetsockname";
    m_names["kOj1HiAGE54"] = "sceNetListen";
    RegisterSymbol("RCCY01Xd+58", reinterpret_cast<uintptr_t>(&Net::sceNetResolverStartNtoaMultipleRecords), "sceNetResolverStartNtoaMultipleRecords");
    RegisterSymbol("TSM6whtekok", reinterpret_cast<uintptr_t>(&Net::sceNetShutdown), "sceNetShutdown");

    // ===== libSceSsl.sprx.json (7 symbols) =====
    RegisterSymbol("RwXD8grHZHM", reinterpret_cast<uintptr_t>(&Http::sceSslFreeSslCertName), "sceSslFreeSslCertName");
    RegisterSymbol("7whYpYfHP74", reinterpret_cast<uintptr_t>(&Http::sceSslGetIssuerName), "sceSslGetIssuerName");
    RegisterSymbol("R1ePzopYPYM", reinterpret_cast<uintptr_t>(&Http::sceSslGetNameEntryCount), "sceSslGetNameEntryCount");
    RegisterSymbol("7RBSTKGrmDA", reinterpret_cast<uintptr_t>(&Http::sceSslGetNameEntryInfo), "sceSslGetNameEntryInfo");
    RegisterSymbol("xHpt6+2pGYk", reinterpret_cast<uintptr_t>(&Http::sceSslGetNotAfter), "sceSslGetNotAfter");
    RegisterSymbol("Eo0S65Jy28Q", reinterpret_cast<uintptr_t>(&Http::sceSslGetNotBefore), "sceSslGetNotBefore");
    RegisterSymbol("dQReuBX9sD8", reinterpret_cast<uintptr_t>(&Http::sceSslGetSubjectName), "sceSslGetSubjectName");

    RegisterSymbol("99b82IKXpH4", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentAppParamGetInt), "sceAppContentAppParamGetInt");
    RegisterSymbol("xnd8BJzAxmk", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentGetAddcontInfoList), "sceAppContentGetAddcontInfoList");
    m_names["XTWR0UXvcgs"] = "sceAppContentGetEntitlementKey";
    RegisterSymbol("R9lA82OraNs", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentInitialize), "sceAppContentInitialize");
    RegisterSymbol("buYbeLOGWmA", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentTemporaryDataMount2");
    RegisterSymbol("bcolXMmp6qQ", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentUmount), "sceAppContentTemporaryDataUnmount");
    RegisterSymbol("VANhIWcqYak", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentAddcontMount");
    RegisterSymbol("7bOLX66Iz-U", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentTemporaryDataMount");
    RegisterSymbol("efX3lrPwdKA", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentAddcontMountByEntitlemetId");
    RegisterSymbol("3rHWaV-1KC4", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentUmount), "sceAppContentAddcontUnmount");
    RegisterSymbol("sceAppContentAddContIdList", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentAddContIdList), "sceAppContentAddContIdList");
    RegisterSymbol("sceAppContentMount", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentMount");
    RegisterSymbol("sceAppContentUmount", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentUmount), "sceAppContentUmount");

    // ===== libScePlayGo.sprx.json (6 symbols) =====
    m_names["Uco1I0dlDi8"] = "scePlayGoClose";
    m_names["uWIYLFkkwqk"] = "scePlayGoGetLocus";
    m_names["-RJWNMK3fC8"] = "scePlayGoGetProgress";
    m_names["ts6GlZOKRrE"] = "scePlayGoInitialize";
    m_names["M1Gma1ocrGE"] = "scePlayGoOpen";
    m_names["MPe0EeBGM-E"] = "scePlayGoTerminate";

    // ===== libSceGameCustomDataDialog.sprx.json (6 symbols) =====
    m_names["sJptZwvs1is"] = "sceGameCustomDataDialogGetResult";
    m_names["xtbb-2f703A"] = "sceGameCustomDataDialogInitialize";
    m_names["5TvttyRuU84"] = "sceGameCustomDataDialogOpen";
    m_names["HwEtHFCpU5U"] = "sceGameCustomDataDialogTerminate";
    m_names["PkdLsRA4ON0"] = "sceGameCustomDataDialogUpdateStatus";
    RegisterSymbol("XDncXQIJUSk", reinterpret_cast<uintptr_t>(&Np::sceNpGetOnlineId), "sceNpGetOnlineId");

    // ===== libScePad.sprx.json (6 symbols) =====
    RegisterSymbol("6ncge5+l5Qs", reinterpret_cast<uintptr_t>(&Pad::scePadClose), "scePadClose");
    RegisterSymbol("gjP9-KQzoUk", reinterpret_cast<uintptr_t>(&Pad::scePadGetControllerInformation), "scePadGetControllerInformation");
    RegisterSymbol("rIZnR6eSpvk", reinterpret_cast<uintptr_t>(&Pad::scePadResetOrientation), "scePadResetOrientation");
    RegisterSymbol("RR4novUEENY", reinterpret_cast<uintptr_t>(&Pad::scePadSetLightBar), "scePadSetLightBar");
    RegisterSymbol("vDLMoJLde8I", reinterpret_cast<uintptr_t>(&Pad::scePadSetTiltCorrectionState), "scePadSetTiltCorrectionState");
    RegisterSymbol("yFVnOdGxvZY", reinterpret_cast<uintptr_t>(&Pad::scePadSetVibration), "scePadSetVibration");

    // ===== libSceMsgDialog.sprx.json (7 symbols) =====
    RegisterSymbol("u4i36qCq7r4", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogClose), "sceMsgDialogClose");
    RegisterSymbol("qKjHjUa40-c", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogGetResult), "sceMsgDialogGetResult");
    RegisterSymbol("Lr8ovHH9l6A", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogGetResult), "sceMsgDialogGetResult");
    RegisterSymbol("Vz8r6m31J94", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogGetStatus), "sceMsgDialogGetStatus");
    RegisterSymbol("CWVW78Qc3fI", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogGetStatus), "sceMsgDialogGetStatus");
    RegisterSymbol("qN8-U+2hO7k", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogInitialize), "sceMsgDialogInitialize");
    RegisterSymbol("lDqxaY1UbEo", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogInitialize), "sceMsgDialogInitialize");
    RegisterSymbol("B3yV9k2gG+4", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogOpen), "sceMsgDialogOpen");
    RegisterSymbol("b06Hh0DPEaE", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogOpen), "sceMsgDialogOpen");
    RegisterSymbol("L7j-3q2s634", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogTerminate), "sceMsgDialogTerminate");
    RegisterSymbol("ePw-kqZmelo", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogTerminate), "sceMsgDialogTerminate");
    RegisterSymbol("6fIC3XKt2k0", reinterpret_cast<uintptr_t>(&Dialog::sceMsgDialogUpdateStatus), "sceMsgDialogUpdateStatus");

    // ===== libSceFont.sprx.json (6 symbols) =====
    RegisterSymbol("L97d+3OgMlE", reinterpret_cast<uintptr_t>(&Font::sceFontGetCharGlyphMetrics), "sceFontGetCharGlyphMetrics");
    RegisterSymbol("BozJej5T6fs", reinterpret_cast<uintptr_t>(&Font::sceFontGetPixelResolution), "sceFontGetPixelResolution");
    RegisterSymbol("ryPlnDDI3rU", reinterpret_cast<uintptr_t>(&Font::sceFontGetRenderScaledKerning), "sceFontGetRenderScaledKerning");
    RegisterSymbol("KXUpebrFk1U", reinterpret_cast<uintptr_t>(&Font::sceFontOpenFontMemory), "sceFontOpenFontMemory");
    RegisterSymbol("kAenWy1Zw5o", reinterpret_cast<uintptr_t>(&Font::sceFontRenderCharGlyphImageHorizontal), "sceFontRenderCharGlyphImageHorizontal");
    RegisterSymbol("N1EBMeGhf7E", reinterpret_cast<uintptr_t>(&Font::sceFontSetScalePixel), "sceFontSetScalePixel");

    // ===== libSceSigninDialog.sprx.json (6 symbols) =====
    RegisterSymbol("M3OkENHcyiU", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogClose), "sceSigninDialogClose");
    RegisterSymbol("nqG7rqnYw1U", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogGetResult), "sceSigninDialogGetResult");
    RegisterSymbol("mlYGfmqE3fQ", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogInitialize), "sceSigninDialogInitialize");
    RegisterSymbol("JlpJVoRWv7U", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogOpen), "sceSigninDialogOpen");
    RegisterSymbol("LXlmS6PvJdU", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogTerminate), "sceSigninDialogTerminate");
    RegisterSymbol("Bw31liTFT3A", reinterpret_cast<uintptr_t>(&Dialog::sceSigninDialogUpdateStatus), "sceSigninDialogUpdateStatus");

    // ===== libSceFiber.sprx.json (5 symbols) =====
    m_names["hVYD7Ou2pCQ"] = "_sceFiberInitializeImpl";
    m_names["JeNX5F-NzQU"] = "sceFiberFinalize";
    m_names["B0ZX2hx9DMw"] = "sceFiberReturnToThread";
    m_names["a0LLrZWac0M"] = "sceFiberRun";
    m_names["PFT2S-tJ7Uk"] = "sceFiberSwitch";

    // ===== libScePlayGoDialog.sprx.json (5 symbols) =====
    RegisterSymbol("fbigNQiZpm0", reinterpret_cast<uintptr_t>(&Dialog::scePlayGoDialogClose), "scePlayGoDialogClose");
    RegisterSymbol("fECamTJKpsM", reinterpret_cast<uintptr_t>(&Dialog::scePlayGoDialogInitialize), "scePlayGoDialogInitialize");
    RegisterSymbol("kHd72ukqbxw", reinterpret_cast<uintptr_t>(&Dialog::scePlayGoDialogOpen), "scePlayGoDialogOpen");
    RegisterSymbol("okgIGdr5Iz0", reinterpret_cast<uintptr_t>(&Dialog::scePlayGoDialogTerminate), "scePlayGoDialogTerminate");
    RegisterSymbol("Yb60K7BST48", reinterpret_cast<uintptr_t>(&Dialog::scePlayGoDialogUpdateStatus), "scePlayGoDialogUpdateStatus");

    // ===== libSceNpCommon.sprx.json (5 symbols) =====
    m_names["i8UmXTSq7N4"] = "sceNpCmpNpId";
    m_names["dj+O5aD2a0Q"] = "sceNpCmpOnlineId";
    RegisterSymbol("NR1J0N7L2xY", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddDays), "sceRtcTickAddDays");
    RegisterSymbol("MDc5cd8HfCA", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddHours), "sceRtcTickAddHours");
    RegisterSymbol("mn-tf4QiFzk", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddMinutes), "sceRtcTickAddMinutes");

    // ===== libScePngDec.sprx.json (5 symbols) =====
    RegisterSymbol("m0uW+8pFyaw", reinterpret_cast<uintptr_t>(&ImageDecoder::scePngDecCreate), "scePngDecCreate");
    RegisterSymbol("WC216DD3El4", reinterpret_cast<uintptr_t>(&ImageDecoder::scePngDecDecode), "scePngDecDecode");
    RegisterSymbol("QbD+eENEwo8", reinterpret_cast<uintptr_t>(&ImageDecoder::scePngDecDelete), "scePngDecDelete");
    RegisterSymbol("U6h4e5JRPaQ", reinterpret_cast<uintptr_t>(&ImageDecoder::scePngDecParseHeader), "scePngDecParseHeader");
    RegisterSymbol("-6srIGbLTIU", reinterpret_cast<uintptr_t>(&ImageDecoder::scePngDecQueryMemorySize), "scePngDecQueryMemorySize");

    // ===== libSceJpegDec.sprx.json (5 symbols) =====
    RegisterSymbol("JPh3Zgg0Zwc", reinterpret_cast<uintptr_t>(&ImageDecoder::sceJpegDecCreate), "sceJpegDecCreate");
    RegisterSymbol("1kzQRoWEgSA", reinterpret_cast<uintptr_t>(&ImageDecoder::sceJpegDecDecode), "sceJpegDecDecode");
    RegisterSymbol("Hwh11+m5KoI", reinterpret_cast<uintptr_t>(&ImageDecoder::sceJpegDecDelete), "sceJpegDecDelete");
    RegisterSymbol("LSinoSQH790", reinterpret_cast<uintptr_t>(&ImageDecoder::sceJpegDecParseHeader), "sceJpegDecParseHeader");
    RegisterSymbol("uNAUmANZMEw", reinterpret_cast<uintptr_t>(&ImageDecoder::sceJpegDecQueryMemorySize), "sceJpegDecQueryMemorySize");

    // ===== libSceErrorDialog.sprx.json (5 symbols) =====
    RegisterSymbol("t2FvHRXzgqk", reinterpret_cast<uintptr_t>(&Dialog::sceErrorDialogGetStatus), "sceErrorDialogGetStatus");
    RegisterSymbol("I88KChlynSs", reinterpret_cast<uintptr_t>(&Dialog::sceErrorDialogInitialize), "sceErrorDialogInitialize");
    RegisterSymbol("M2ZF-ClLhgY", reinterpret_cast<uintptr_t>(&Dialog::sceErrorDialogOpen), "sceErrorDialogOpen");
    RegisterSymbol("9XAxK2PMwk8", reinterpret_cast<uintptr_t>(&Dialog::sceErrorDialogTerminate), "sceErrorDialogTerminate");
    RegisterSymbol("WWiGuh9XfgQ", reinterpret_cast<uintptr_t>(&Dialog::sceErrorDialogUpdateStatus), "sceErrorDialogUpdateStatus");

    // ===== libSceNpCommerce.sprx.json (5 symbols) =====
    m_names["r42bWcQbtZY"] = "sceNpCommerceDialogGetResult";
    m_names["0aR2aWmQal4"] = "sceNpCommerceDialogInitialize";
    m_names["DfSCDRA3EjY"] = "sceNpCommerceDialogOpen";
    m_names["m-I92Ab50W8"] = "sceNpCommerceDialogTerminate";
    m_names["LR5cwFMMCVE"] = "sceNpCommerceDialogUpdateStatus";

    // ===== libSceInvitationDialog.sprx.json (5 symbols) =====
    m_names["8XKR6wa64iQ"] = "sceInvitationDialogGetResult";
    m_names["XvA5KS56wcs"] = "sceInvitationDialogInitialize";
    m_names["0zU0G+wiVLA"] = "sceInvitationDialogOpen";
    m_names["B6HVJtDYxEE"] = "sceInvitationDialogTerminate";
    m_names["9+g9iOq+7kg"] = "sceInvitationDialogUpdateStatus";

    // ===== libSceNpSnsFacebookDialog.sprx.json (5 symbols) =====
    m_names["AifdcI1aKas"] = "sceNpSnsFacebookDialogGetResult";
    m_names["tdQArULbNQk"] = "sceNpSnsFacebookDialogInitialize";
    m_names["Hi3onqHSN6M"] = "sceNpSnsFacebookDialogOpen";
    m_names["cdqOFyupRS4"] = "sceNpSnsFacebookDialogTerminate";
    m_names["fjV7C8H0Y8k"] = "sceNpSnsFacebookDialogUpdateStatus";

    // ===== libSceJpegEnc.sprx.json (4 symbols) =====
    m_names["K+rocojkr-I"] = "sceJpegEncCreate";
    m_names["j1LyMdaM+C0"] = "sceJpegEncDelete";
    m_names["QbrU0cUghEM"] = "sceJpegEncEncode";
    m_names["o6ZgXfFdWXQ"] = "sceJpegEncQueryMemorySize";

    // ===== libSceCdlgUtilServer.sprx.json (4 symbols) =====
    RegisterSymbol("WJ3rqFwymew", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcFormatRfc3339), "sceRtcFormatRFC3339");
    RegisterSymbol("18B2NS1y9UU", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetCurrentNetworkTick), "sceRtcGetCurrentTick");
    m_names["jMNwqYr4R-k"] = "sceRtcGetTickResolution";
    RegisterSymbol("99bMGglFW3I", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcParseRfc3339), "sceRtcParseRFC3339");

    // ===== libSceRtc.sprx.json (4 symbols) =====
    m_names["eiuobaF-hK4"] = "sceRtcFormatRFC2822";
    m_names["AxHBk3eat04"] = "sceRtcFormatRFC2822LocalTime";
    m_names["CL6y9q-XbuQ"] = "sceRtcTickAddMonths";
    m_names["-5y2uJ62qS8"] = "sceRtcTickAddYears";

    // ===== libSceNpEulaDialog.sprx.json (4 symbols) =====
    m_names["GpLQDNKICac"] = "sceNpCreateRequest";
    RegisterSymbol("S7QTn72PrDw", reinterpret_cast<uintptr_t>(&Np::sceNpDeleteRequest), "sceNpDeleteRequest");
    m_names["Ghz9iWDUtC4"] = "sceNpGetAccountCountry";
    m_names["KZ1Mj9yEGYc"] = "sceNpGetAccountLanguage";

    // ===== libSceShareUtility.sprx.json (4 symbols) =====
    m_names["Jlv-lYxEnvM"] = "sceShareUtilityInitialize";
    m_names["Lqp4EDIRXSo"] = "sceShareUtilityOpenShareMenu";
    m_names["8hZ2EEl2Tto"] = "sceShareUtilityOpenShareMenuDefault";
    m_names["DUWhxkyVPj4"] = "sceShareUtilityTerminate";

    // ===== libSceVideoCoreInterface.sprx.json (4 symbols) =====
    RegisterSymbol("HXzjK9yI30k", reinterpret_cast<uintptr_t>(&Video::sceVideoOutAddFlipEvent), "sceVideoOutAddFlipEvent");
    RegisterSymbol("Up36PTk687E", reinterpret_cast<uintptr_t>(&Video::sceVideoOutOpen), "sceVideoOutOpen");
    RegisterSymbol("w3BY+tAEiQY", reinterpret_cast<uintptr_t>(&Video::sceVideoOutRegisterBuffers), "sceVideoOutRegisterBuffers");
    RegisterSymbol("i6-sR91Wt-4", reinterpret_cast<uintptr_t>(&Video::sceVideoOutSetBufferAttribute), "sceVideoOutSetBufferAttribute");

    // ===== libSceAbstractTwitter.sprx.json (3 symbols) =====
    RegisterSymbol("zO9UL3qIINQ", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetCurrentNetworkTick), "sceRtcGetCurrentNetworkTick");
    m_names["BtqmpTRXHgk"] = "sceRtcGetTime_t";
    m_names["ueega6v3GUw"] = "sceRtcSetTick";

    // ===== libSceContentDelete.sprx.json (3 symbols) =====
    m_names["pXJh3aVk8Ks"] = "sceContentDeleteById";
    m_names["zoxb0wEChEM"] = "sceContentDeleteInitialize";
    m_names["5XLSih32qHA"] = "sceContentDeleteTerminate";

    // ===== libSceGnmDriver.sprx.json (3 symbols) =====
    m_names["5q95ravnueg"] = "sceGnmDrawIndexIndirectMulti";
    m_names["f5QQLp9rzGk"] = "sceGnmDrawIndirectMulti";
    m_names["8lH54sfjfmU"] = "sceGnmDrawInitToDefaultContextState";

    // ===== libSceAudioOut.sprx.json (3 symbols) =====
    m_names["GrQ9s4IrNaQ"] = "sceAudioOutGetPortState";
    m_names["w3PdaSTSwGE"] = "sceAudioOutOutputs";
    RegisterSymbol("wVwPU50pS1c", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutSetMixLevelPadSpk), "sceAudioOutSetMixLevelPadSpk");

    // ===== libSceNpAuth.sprx.json (3 symbols) =====
    m_names["6bwFkosYRQg"] = "sceNpAuthCreateRequest";
    m_names["H8wG9Bk-nPc"] = "sceNpAuthDeleteRequest";
    m_names["KxGkOrQJTqY"] = "sceNpAuthGetAuthorizationCode";

    // ===== libSceNpSns.sprx.json (3 symbols) =====
    m_names["OoTjfxl8-wI"] = "sceNpSnsFacebookCreateRequest";
    m_names["pTvAKV1iQkE"] = "sceNpSnsFacebookDeleteRequest";
    m_names["VtyS8XLBqNE"] = "sceNpSnsFacebookGetAccessToken";

    // ===== libSceSystemService.sprx.json (2 symbols) =====
    RegisterSymbol("Vo5V8KAwCmk", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceHideSplashScreen), "sceSystemServiceHideSplashScreen");
    RegisterSymbol("656LMQSrg6U", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceReceiveEvent), "sceSystemServiceReceiveEvent");

    // ===== libSceAbstractDailymotion.sprx.json (2 symbols) =====
    RegisterSymbol("YuOW3dDAKYc", reinterpret_cast<uintptr_t>(&Http::sceHttpUriEscape), "sceHttpUriEscape");
    RegisterSymbol("8w-H19ip48I", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetTick), "sceRtcGetTick");

    // ===== libSceGvMp4Parser.sprx.json (2 symbols) =====
    RegisterSymbol("8Yr143yEnRo", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcConvertLocalTimeToUtc), "sceRtcConvertLocalTimeToUtc");
    RegisterSymbol("M1TvFst-jrM", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcConvertUtcToLocalTime), "sceRtcConvertUtcToLocalTime");

    // ===== libSceOrbisCompat.sprx.json (2 symbols) =====
    RegisterSymbol("xk0AcarP3V4", reinterpret_cast<uintptr_t>(&Pad::scePadOpen), "scePadOpen");
    RegisterSymbol("YndgXqQVV7c", reinterpret_cast<uintptr_t>(&Pad::scePadReadState), "scePadReadState");

    // ===== libSceAvPlayerStreaming.sprx.json (2 symbols) =====
    RegisterSymbol("qFg2SuyTJJY", reinterpret_cast<uintptr_t>(&Http::sceHttpSetAuthEnabled), "sceHttpSetAuthEnabled");
    RegisterSymbol("NxEI1KByvCI", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcParseDateTime), "sceRtcParseDateTime");

    // ===== libSceUserService.sprx.json (2 symbols) =====
    RegisterSymbol("yH17Q6NWtVg", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetEvent), "sceUserServiceGetEvent");
    RegisterSymbol("1xxcMiGu2fo", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetUserName), "sceUserServiceGetUserName");

    // ===== libSceNgs2.sprx.json (2 symbols) =====
    m_names["cLV4aiT9JpA"] = "sceNgs2RackCreate";
    RegisterSymbol("0eFLVCfWVds", reinterpret_cast<uintptr_t>(&Audio::sceNgs2RackQueryBufferSize), "sceNgs2RackQueryBufferSize");

    // ===== libmonosgen-2.0.sprx.json (2 symbols) =====
    m_names["HQOwnfMGipQ"] = "sceNetErrnoLoc";
    m_names["C4UgDHHPvdw"] = "sceNetResolverCreate";

    // ===== libSceCompanionHttpd.sprx.json (2 symbols) =====
    RegisterSymbol("obuxdTiwkF8", reinterpret_cast<uintptr_t>(&Net::sceNetCtlGetInfo), "sceNetCtlGetInfo");
    RegisterSymbol("gky0+oaNM4k", reinterpret_cast<uintptr_t>(&Net::sceNetCtlInit), "sceNetCtlInit");

    // ===== libSceCommonDialog.sprx.json (1 symbols) =====
    RegisterSymbol("uoUpLGNkygk", reinterpret_cast<uintptr_t>(&Dialog::sceCommonDialogInitialize), "sceCommonDialogInitialize");

    // ===== libSceGameLiveStreaming.sprx.json (1 symbols) =====
    RegisterSymbol("rPo6tV8D9bM", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceGetStatus), "sceSystemServiceGetStatus");

    // ===== libSceOrbisCompatForVideoService.sprx.json (1 symbols) =====
    RegisterSymbol("hv1luiJrqQM", reinterpret_cast<uintptr_t>(&Pad::scePadInit), "scePadInit");

    // ===== libSceHmd.sprx.json (1 symbols) =====
    m_names["6kPnj51T62Y"] = "sceVideoOutGetResolutionStatus";

    // ===== libSceVideoOut.sprx.json (1 symbols) =====
    RegisterSymbol("CBiu4mCE1DA", reinterpret_cast<uintptr_t>(&Video::sceVideoOutSetFlipRate), "sceVideoOutSetFlipRate");

    // ===== libSceAbstractStorage.sprx.json (1 symbols) =====
    m_names["2mKX2Spso7I"] = "sceNetSetsockopt";

    // ===== libc.sprx.json (1 symbols) =====
    m_names["NWtTN10cJzE"] = "sceLibcHeapGetTraceInfo";

    // ===== libSceDiscMap.sprx.json (1 symbols) =====
    m_names["lbQKqsERhtE"] = "sceDiscMapIsRequestOnHDD";

    // ===== libSceNetCtl.sprx.json (1 symbols) =====
    RegisterSymbol("JO4yuTuMoKI", reinterpret_cast<uintptr_t>(&Net::sceNetCtlGetNatInfo), "sceNetCtlGetNatInfo");

    // ===== Named NP & Trophy Offline Subsystem Symbol Registrations =====
    RegisterSymbol("sceNpManagerGetStatus", reinterpret_cast<uintptr_t>(&Np::sceNpManagerGetStatus), "sceNpManagerGetStatus");
    RegisterSymbol("sceNpManagerGetOnlineId", reinterpret_cast<uintptr_t>(&Np::sceNpManagerGetOnlineId), "sceNpManagerGetOnlineId");
    RegisterSymbol("sceNpManagerGetNpId", reinterpret_cast<uintptr_t>(&Np::sceNpManagerGetNpId), "sceNpManagerGetNpId");
    RegisterSymbol("sceNpManagerGetAccountId", reinterpret_cast<uintptr_t>(&Np::sceNpManagerGetAccountId), "sceNpManagerGetAccountId");
    RegisterSymbol("sceNpCheckCallback", reinterpret_cast<uintptr_t>(&Np::sceNpCheckCallback), "sceNpCheckCallback");
    RegisterSymbol("sceNpTrophyInit", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyInit), "sceNpTrophyInit");
    RegisterSymbol("sceNpTrophyTerm", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyTerm), "sceNpTrophyTerm");
    RegisterSymbol("sceNpTrophyCreateContext", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyCreateContext), "sceNpTrophyCreateContext");
    RegisterSymbol("sceNpTrophyDestroyContext", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyDestroyContext), "sceNpTrophyDestroyContext");
    RegisterSymbol("sceNpTrophyCreateHandle", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyCreateHandle), "sceNpTrophyCreateHandle");
    RegisterSymbol("sceNpTrophyDestroyHandle", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyDestroyHandle), "sceNpTrophyDestroyHandle");
    RegisterSymbol("sceNpTrophyRegisterContext", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyRegisterContext), "sceNpTrophyRegisterContext");
    RegisterSymbol("sceNpTrophyUnlockTrophy", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyUnlockTrophy), "sceNpTrophyUnlockTrophy");
    RegisterSymbol("sceNpTrophyGetTrophyUnlockState", reinterpret_cast<uintptr_t>(&Np::sceNpTrophyGetTrophyUnlockState), "sceNpTrophyGetTrophyUnlockState");
    RegisterSymbol("sceNpMatching2Initialize", reinterpret_cast<uintptr_t>(&Np::sceNpMatching2Initialize), "sceNpMatching2Initialize");
    RegisterSymbol("sceNpMatching2Terminate", reinterpret_cast<uintptr_t>(&Np::sceNpMatching2Terminate), "sceNpMatching2Terminate");
    RegisterSymbol("sceNpScoreRankingInitialize", reinterpret_cast<uintptr_t>(&Np::sceNpScoreRankingInitialize), "sceNpScoreRankingInitialize");
    // ===== User, System Service, and AppContent Named Registrations =====
    RegisterSymbol("sceUserServiceInitialize", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceInitialize), "sceUserServiceInitialize");
    RegisterSymbol("sceUserServiceTerminate", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceTerminate), "sceUserServiceTerminate");
    RegisterSymbol("sceUserServiceGetInitialUser", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetInitialUser), "sceUserServiceGetInitialUser");
    RegisterSymbol("sceUserServiceGetLoginUserIdList", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetLoginUserIdList), "sceUserServiceGetLoginUserIdList");
    RegisterSymbol("sceUserServiceGetUserName", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetUserName), "sceUserServiceGetUserName");
    RegisterSymbol("sceUserServiceRegisterEventCallback", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceRegisterEventCallback), "sceUserServiceRegisterEventCallback");
    RegisterSymbol("sceUserServiceUnregisterEventCallback", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceUnregisterEventCallback), "sceUserServiceUnregisterEventCallback");
    RegisterSymbol("sceUserServiceGetEvent", reinterpret_cast<uintptr_t>(&Kernel::User::sceUserServiceGetEvent), "sceUserServiceGetEvent");
    RegisterSymbol("sceSystemServiceGetStatus", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceGetStatus), "sceSystemServiceGetStatus");
    RegisterSymbol("sceSystemServiceReceiveEvent", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceReceiveEvent), "sceSystemServiceReceiveEvent");
    RegisterSymbol("sceSystemServiceHideSplashScreen", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceHideSplashScreen), "sceSystemServiceHideSplashScreen");
    RegisterSymbol("sceSystemServiceParamGetInt", reinterpret_cast<uintptr_t>(&Kernel::sceSystemServiceParamGetInt), "sceSystemServiceParamGetInt");
    RegisterSymbol("sceAppContentAddContIdList", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentAddContIdList), "sceAppContentAddContIdList");
    RegisterSymbol("sceAppContentMount", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentMount), "sceAppContentMount");
    RegisterSymbol("sceAppContentUmount", reinterpret_cast<uintptr_t>(&Kernel::sceAppContentUmount), "sceAppContentUmount");

    // ===== AvPlayer Named Registrations =====
    RegisterSymbol("sceAvPlayerInit", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerInit), "sceAvPlayerInit");
    RegisterSymbol("sceAvPlayerPostInit", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerPostInit), "sceAvPlayerPostInit");
    RegisterSymbol("sceAvPlayerClose", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerClose), "sceAvPlayerClose");
    RegisterSymbol("sceAvPlayerAddSource", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerAddSource), "sceAvPlayerAddSource");
    RegisterSymbol("sceAvPlayerStart", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStart), "sceAvPlayerStart");
    RegisterSymbol("sceAvPlayerStop", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStop), "sceAvPlayerStop");
    RegisterSymbol("sceAvPlayerPause", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerPause), "sceAvPlayerPause");
    RegisterSymbol("sceAvPlayerResume", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerResume), "sceAvPlayerResume");
    RegisterSymbol("sceAvPlayerIsActive", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerIsActive), "sceAvPlayerIsActive");
    RegisterSymbol("sceAvPlayerEnableStream", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerEnableStream), "sceAvPlayerEnableStream");
    RegisterSymbol("sceAvPlayerStreamCount", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerStreamCount), "sceAvPlayerStreamCount");
    RegisterSymbol("sceAvPlayerGetStreamInfo", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerGetStreamInfo), "sceAvPlayerGetStreamInfo");
    RegisterSymbol("sceAvPlayerGetVideoDataEx", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerGetVideoDataEx), "sceAvPlayerGetVideoDataEx");
    RegisterSymbol("sceAvPlayerSetLooping", reinterpret_cast<uintptr_t>(&Video::AvPlayer::sceAvPlayerSetLooping), "sceAvPlayerSetLooping");

    // ===== RTC Named Registrations =====
    RegisterSymbol("sceRtcGetCurrentNetworkTick", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetCurrentNetworkTick), "sceRtcGetCurrentNetworkTick");
    RegisterSymbol("sceRtcGetTick", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcGetTick), "sceRtcGetTick");
    RegisterSymbol("sceRtcTickAddDays", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddDays), "sceRtcTickAddDays");
    RegisterSymbol("sceRtcTickAddHours", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddHours), "sceRtcTickAddHours");
    RegisterSymbol("sceRtcTickAddMinutes", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddMinutes), "sceRtcTickAddMinutes");
    RegisterSymbol("sceRtcTickAddSeconds", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcTickAddSeconds), "sceRtcTickAddSeconds");
    RegisterSymbol("sceRtcConvertUtcToLocalTime", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcConvertUtcToLocalTime), "sceRtcConvertUtcToLocalTime");
    RegisterSymbol("sceRtcConvertLocalTimeToUtc", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcConvertLocalTimeToUtc), "sceRtcConvertLocalTimeToUtc");
    RegisterSymbol("sceRtcFormatRfc3339", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcFormatRfc3339), "sceRtcFormatRfc3339");
    RegisterSymbol("sceRtcFormatRFC3339", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcFormatRFC3339), "sceRtcFormatRFC3339");
    RegisterSymbol("sceRtcParseRfc3339", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcParseRfc3339), "sceRtcParseRfc3339");
    RegisterSymbol("sceRtcParseRFC3339", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcParseRFC3339), "sceRtcParseRFC3339");
    RegisterSymbol("sceRtcParseDateTime", reinterpret_cast<uintptr_t>(&Kernel::Rtc::sceRtcParseDateTime), "sceRtcParseDateTime");

    // ===== GNM Named Registrations =====
    RegisterSymbol("sceGnmAreSubmitsAllowed", reinterpret_cast<uintptr_t>(&GPU::sceGnmAreSubmitsAllowed), "sceGnmAreSubmitsAllowed");
    RegisterSymbol("sceGnmBeginWorkload", reinterpret_cast<uintptr_t>(&GPU::sceGnmBeginWorkload), "sceGnmBeginWorkload");
    RegisterSymbol("sceGnmEndWorkload", reinterpret_cast<uintptr_t>(&GPU::sceGnmEndWorkload), "sceGnmEndWorkload");
    RegisterSymbol("sceGnmAddEqEvent", reinterpret_cast<uintptr_t>(&GPU::sceGnmAddEqEvent), "sceGnmAddEqEvent");
    RegisterSymbol("sceGnmDeleteEqEvent", reinterpret_cast<uintptr_t>(&GPU::sceGnmDeleteEqEvent), "sceGnmDeleteEqEvent");
    RegisterSymbol("sceGnmGetGpuBlockStatus", reinterpret_cast<uintptr_t>(&GPU::sceGnmGetGpuBlockStatus), "sceGnmGetGpuBlockStatus");
    RegisterSymbol("sceGnmDrawIndex", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndex), "sceGnmDrawIndex");
    RegisterSymbol("sceGnmDrawIndexOffset", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexOffset), "sceGnmDrawIndexOffset");
    RegisterSymbol("sceGnmDrawIndirect", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndirect), "sceGnmDrawIndirect");
    RegisterSymbol("sceGnmDrawIndexIndirect", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexIndirect), "sceGnmDrawIndexIndirect");
    RegisterSymbol("sceGnmDrawIndexMultiInstanced", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawIndexMultiInstanced), "sceGnmDrawIndexMultiInstanced");

    // ===== Security Named Registrations =====
    RegisterSymbol("sceLibSecureInit", reinterpret_cast<uintptr_t>(&Security::sceLibSecureInit), "sceLibSecureInit");
    RegisterSymbol("sceLibSecureDestroy", reinterpret_cast<uintptr_t>(&Security::sceLibSecureDestroy), "sceLibSecureDestroy");
    RegisterSymbol("sceLibSecureCryptographyGetContextSize", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyGetContextSize), "sceLibSecureCryptographyGetContextSize");
    RegisterSymbol("sceLibSecureCryptographySetContext", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographySetContext), "sceLibSecureCryptographySetContext");
    RegisterSymbol("sceLibSecureCryptographyEncrypt", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyEncrypt), "sceLibSecureCryptographyEncrypt");
    RegisterSymbol("sceLibSecureCryptographyDecrypt", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyDecrypt), "sceLibSecureCryptographyDecrypt");
    RegisterSymbol("sceLibSecureCryptographyDeleteContext", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyDeleteContext), "sceLibSecureCryptographyDeleteContext");
    RegisterSymbol("sceLibSecureCryptographyMessagePadding", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyMessagePadding), "sceLibSecureCryptographyMessagePadding");
    RegisterSymbol("sceLibSecureCryptographyMessagePaddingSize", reinterpret_cast<uintptr_t>(&Security::sceLibSecureCryptographyMessagePaddingSize), "sceLibSecureCryptographyMessagePaddingSize");

    LOG_INFO("NID", "Registered " + std::to_string(m_symbols.size()) + " native host API symbols and " +
             std::to_string(m_names.size()) + " named API stubs.");
}

} // namespace Loader
