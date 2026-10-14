#include "loader/nid_table.h"
#include "common/logger.h"
#include "kernel/libkernel.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
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
    RegisterSymbol("yH17Q6NWtVg", reinterpret_cast<uintptr_t>(&Kernel::sceUserServiceGetEvent), "sceUserServiceGetEvent");
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
    m_names["JfEPXVxhFqA"] = "sceAudioOutInit";
    RegisterSymbol("ekNvsT22rsY", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOpen), "sceAudioOutOpen");
    RegisterSymbol("QOQtbeDqsT4", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOutput), "sceAudioOutOutput");
    RegisterSymbol("w3PdaSTSwGE", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOutputs), "sceAudioOutOutputs");
    RegisterSymbol("GrQ9s4IrNaQ", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutGetPortState), "sceAudioOutGetPortState");
    RegisterSymbol("YaaDbDwKpFM", reinterpret_cast<uintptr_t>(&Audio::sceAudio3dPortGetQueueLevel), "sceAudio3dPortGetQueueLevel");
    RegisterSymbol("i0VnXM-C9fc", reinterpret_cast<uintptr_t>(&Audio::sceNgs2SystemRender), "sceNgs2SystemRender");
    m_names["3OdRkSjOcog"] = "sceFontBindRenderer";
    m_names["vzHs3C8lWJk"] = "sceFontCloseFont";
    m_names["n590hj5Oe-k"] = "sceFontCreateLibraryWithEdition";
    m_names["WaSFJoRWXaI"] = "sceFontCreateRendererWithEdition";
    m_names["FXP359ygujs"] = "sceFontDestroyLibrary";
    m_names["exAxkyVLt0s"] = "sceFontDestroyRenderer";
    m_names["imxVx8lm+KM"] = "sceFontGetHorizontalLayout";
    m_names["whrS4oksXc4"] = "sceFontMemoryInit";
    m_names["h6hIgxXEiEc"] = "sceFontMemoryTerm";
    m_names["gdUCnU0gHdI"] = "sceFontRenderSurfaceInit";
    m_names["oM+XCzVG3oM"] = "sceFontSelectLibraryFt";
    m_names["Xx974EW-QFY"] = "sceFontSelectRendererFt";
    m_names["6vGCkkQJOcI"] = "sceFontSetupRenderScalePixel";
    m_names["mz2iTY0MK4A"] = "sceFontSupportExternalFonts";
    m_names["1QjhKxrsOB8"] = "sceFontUnbindRenderer";
    m_names["b0xyllnVY-I"] = "sceGnmAddEqEvent";
    m_names["b08AgtPlHPg"] = "sceGnmAreSubmitsAllowed";
    m_names["ihxrbsoSKWc"] = "sceGnmBeginWorkload";
    m_names["5udAm+6boVg"] = "sceGnmCreateWorkloadStream";
    m_names["qpGITzPE+Zc"] = "sceGnmDebugHardwareStatus";
    m_names["PVT+fuoS9gU"] = "sceGnmDeleteEqEvent";
    m_names["UtObDRQiGbs"] = "sceGnmDestroyWorkloadStream";
    RegisterSymbol("bX5IbRvECXk", reinterpret_cast<uintptr_t>(&GPU::sceGnmDingDong), "sceGnmDingDong");
    RegisterSymbol("byXlqupd8cE", reinterpret_cast<uintptr_t>(&GPU::sceGnmDingDong), "sceGnmDingDongForWorkload");
    m_names["HHo1BAljZO8"] = "sceGnmDisableMipStatsReport";
    RegisterSymbol("0BzLGljcwBo", reinterpret_cast<uintptr_t>(&GPU::sceGnmDispatchDirect), "sceGnmDispatchDirect");
    m_names["Z43vKp5k7r0"] = "sceGnmDispatchIndirect";
    m_names["wED4ZXCFJT0"] = "sceGnmDispatchIndirectOnMec";
    m_names["nF6bFRUBRAU"] = "sceGnmDispatchInitDefaultHardwareState";
    m_names["HlTPoZ-oY7Y"] = "sceGnmDrawIndex";
    m_names["GGsn7jMTxw4"] = "sceGnmDrawIndexAuto";
    m_names["ED9-Fjr8Ta4"] = "sceGnmDrawIndexIndirect";
    m_names["jHdPvIzlpKc"] = "sceGnmDrawIndexMultiInstanced";
    m_names["oYM+YzfCm2Y"] = "sceGnmDrawIndexOffset";
    m_names["4v+otIIdjqg"] = "sceGnmDrawIndirect";
    m_names["yb2cRhagD1I"] = "sceGnmDrawInitDefaultHardwareState350";
    m_names["stDSYW2SBVM"] = "sceGnmDrawOpaqueAuto";
    m_names["TLV4mswiZ4A"] = "sceGnmDriverCaptureInProgress";
    m_names["R6z1xM3pW-w"] = "sceGnmDriverTraceInProgress";
    m_names["d88anrgNoKY"] = "sceGnmDriverTriggerCapture";
    m_names["Fa3x75OOLRA"] = "sceGnmEndWorkload";
    RegisterSymbol("iBt3Oe00Kvc", reinterpret_cast<uintptr_t>(&GPU::sceGnmFlushGarlic), "sceGnmFlushGarlic");
    m_names["UoYY0DWMC0U"] = "sceGnmGetEqEventType";
    m_names["H7-fgvEutM0"] = "sceGnmGetEqTimeStamp";
    m_names["oL4hGI1PMpw"] = "sceGnmGetGpuBlockStatus";
    m_names["tZCSL5ulnB4"] = "sceGnmGetGpuInfoStatus";
    m_names["iFirFzgYsvw"] = "sceGnmGetLastWaitedAddress";
    m_names["KnldROUkWJY"] = "sceGnmGetNumTcaUnits";
    m_names["fzJdEihTFV4"] = "sceGnmGetProtectionFaultTimeStamp";
    m_names["nEyFbYUloIM"] = "sceGnmGetShaderProgramBaseAddress";
    m_names["k7iGTvDQPLQ"] = "sceGnmGetShaderStatus";
    m_names["ln33zjBrfjk"] = "sceGnmGetTheTessellationFactorRingBufferBaseAddress";
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
    m_names["29oKvKXzEZo"] = "sceGnmMapComputeQueue";
    m_names["A+uGq+3KFtQ"] = "sceGnmMapComputeQueueWithPriority";
    m_names["gObODli-OH8"] = "sceGnmRequestFlipAndSubmitDone";
    m_names["6YRHhh5mHCs"] = "sceGnmRequestFlipAndSubmitDoneForWorkload";
    m_names["f85orjx7qts"] = "sceGnmRequestMipStatsReportAndReset";
    m_names["MYRtYhojKdA"] = "sceGnmResetVgtControl";
    m_names["Kx-h-nWQJ8A"] = "sceGnmSetCsShaderWithModifier";
    m_names["X9Omw9dwv5M"] = "sceGnmSetEmbeddedPsShader";
    m_names["+AFvOEXrKJk"] = "sceGnmSetEmbeddedVsShader";
    m_names["FUHG8sQ3R58"] = "sceGnmSetEsShader";
    m_names["jtkqXpAOY6w"] = "sceGnmSetGsRingSizes";
    m_names["UJwNuMBcUAk"] = "sceGnmSetGsShader";
    m_names["VJNjFtqiF5w"] = "sceGnmSetHsShader";
    m_names["vckdzbQ46SI"] = "sceGnmSetLsShader";
    m_names["5uFKckiJYRM"] = "sceGnmSetPsShader350";
    m_names["cFCp0NX8wf0"] = "sceGnmSetVgtControl";
    RegisterSymbol("gAhCn6UiU4Y", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetVsShader), "sceGnmSetVsShader");
    m_names["XiyzNZ9J4nQ"] = "sceGnmSetWaveLimitMultipliers";
    m_names["+xuDhxlWRPg"] = "sceGnmSetupMipStatsReport";
    RegisterSymbol("xbxNatawohc", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitAndFlipCommandBuffers), "sceGnmSubmitAndFlipCommandBuffers");
    RegisterSymbol("Ga6r7H6Y0RI", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitAndFlipCommandBuffers), "sceGnmSubmitAndFlipCommandBuffersForWorkload");
    RegisterSymbol("zwY0YV91TTI", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitCommandBuffers), "sceGnmSubmitCommandBuffers");
    m_names["jRcI8VcgTz4"] = "sceGnmSubmitCommandBuffersForWorkload";
    RegisterSymbol("yvZ73uQUqrk", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitDone), "sceGnmSubmitDone");
    m_names["ArSg-TGinhk"] = "sceGnmUnmapComputeQueue";
    m_names["nLM2i2+65hA"] = "sceGnmUpdateGsShader";
    m_names["GNlx+y7xPdE"] = "sceGnmUpdateHsShader";
    m_names["mLVL7N7BVBg"] = "sceGnmUpdatePsShader350";
    m_names["V31V01UiScY"] = "sceGnmUpdateVsShader";
    m_names["hvG6GfBMXg8"] = "sceHttpAbortRequest";
    m_names["EY28T2bkN7k"] = "sceHttpAddRequestHeader";
    m_names["qgxDBjorUxs"] = "sceHttpCreateConnectionWithURL";
    m_names["Aeu5wVKkF9w"] = "sceHttpCreateRequestWithURL";
    m_names["0gYjPTR-6cY"] = "sceHttpCreateTemplate";
    m_names["P6A3ytpsiYc"] = "sceHttpDeleteConnection";
    m_names["qe7oZ+v4PWA"] = "sceHttpDeleteRequest";
    m_names["4I8vEpuEhZ8"] = "sceHttpDeleteTemplate";
    m_names["aCYPMSUIaP8"] = "sceHttpGetAllResponseHeaders";
    m_names["0onIrKx9NIE"] = "sceHttpGetLastErrno";
    m_names["yuO2H2Uvnos"] = "sceHttpGetResponseContentLength";
    m_names["0a2TBNfE3BU"] = "sceHttpGetStatusCode";
    m_names["A9cVMUtEp4Y"] = "sceHttpInit";
    m_names["hPTXo3bICzI"] = "sceHttpParseResponseHeader";
    m_names["P5pdoykPYTk"] = "sceHttpReadData";
    m_names["1e2BNwI-XzE"] = "sceHttpSendRequest";
    m_names["T-mGo9f3Pu4"] = "sceHttpSetAutoRedirect";
    m_names["0S9tTH0uqTU"] = "sceHttpSetConnectTimeOut";
    m_names["yigr4V0-HTM"] = "sceHttpSetRecvTimeOut";
    m_names["K1d1LqZRQHQ"] = "sceHttpSetResolveRetry";
    m_names["Tc-hAYDKtQc"] = "sceHttpSetResolveTimeOut";
    m_names["xegFfZKBVlw"] = "sceHttpSetSendTimeOut";
    m_names["Ik-KpLTlf7Q"] = "sceHttpTerm";
    m_names["JBN6N-EY+3M"] = "sceHttpsGetSslError";
    m_names["htyBOoWeS58"] = "sceHttpsSetSslCallback";
    m_names["hMYgMP-Vuno"] = "sceLibSecureCryptographyDecrypt";
    m_names["RsiCFJMIGHw"] = "sceLibSecureCryptographyDeleteContext";
    m_names["aEoi0u2FOiQ"] = "sceLibSecureCryptographyEncrypt";
    m_names["pT4ViD0o-bM"] = "sceLibSecureCryptographyGetContextSize";
    m_names["XKDzFiGAvhU"] = "sceLibSecureCryptographyMessagePadding";
    m_names["NpcegFLFQpg"] = "sceLibSecureCryptographyMessagePaddingSize";
    m_names["hyfPWjJB8pI"] = "sceLibSecureCryptographySetContext";
    m_names["t4lnn4rB+U0"] = "sceLibSecureDestroy";
    m_names["fIqNvkKIUPQ"] = "sceLibSecureInit";
    m_names["iQw3iQPhvUQ"] = "sceNetCtlCheckCallback";
    m_names["0cBgduPRR+M"] = "sceNetCtlGetResult";
    m_names["uBPlr0lbuiI"] = "sceNetCtlGetState";
    m_names["UJ+Z7Q+4ck0"] = "sceNetCtlRegisterCallback";
    m_names["Rqm2OnZMCz0"] = "sceNetCtlUnregisterCallback";
    m_names["Nlev7Lg8k3A"] = "sceNetInit";
    m_names["dgJBaeJnGpo"] = "sceNetPoolCreate";
    m_names["K7RlrTkI-mw"] = "sceNetPoolDestroy";
    m_names["cTGkc6-TBlI"] = "sceNetTerm";
    m_names["U546k6orxQo"] = "sceNgs2RackCreateWithAllocator";
    m_names["lCqD7oycmIM"] = "sceNgs2RackDestroy";
    m_names["MwmHz8pAdAo"] = "sceNgs2RackGetVoiceHandle";
    m_names["mPYgU4oYpuY"] = "sceNgs2SystemCreateWithAllocator";
    m_names["u-WrYDaJA3k"] = "sceNgs2SystemDestroy";
    m_names["i0VnXM-C9fc"] = "sceNgs2SystemRender";
    m_names["uu94irFOGpA"] = "sceNgs2VoiceControl";
    m_names["-TOuuAQ-buE"] = "sceNgs2VoiceGetState";
    m_names["u9YKpRRHe-M"] = "sceRazorCaptureImmediate";
    m_names["f33OrruQYbM"] = "sceRazorIsLoaded";
    m_names["DOwXL+FQMEY"] = "sceSslGetSerialNumber";
    m_names["hdpVEUDFW3s"] = "sceSslInit";
    m_names["0K1yQ6Lv-Yc"] = "sceSslTerm";
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
    m_names["10t3e5+JPnU"] = "sceNpMatching2Initialize";
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
    m_names["Mqp3lJ+sjy4"] = "sceNpMatching2Terminate";
    m_names["-PoIzr3PEk0"] = "sceSslGetMemoryPoolStats";

    // ===== libSceNpManager.sprx.json (25 symbols) =====
    m_names["OzKvTvg3ZYU"] = "sceNpAbortRequest";
    m_names["3Zl8BePTh9Y"] = "sceNpCheckCallback";
    m_names["JELHf4xPufo"] = "sceNpCheckCallbackForLib";
    m_names["2rsFmlGWleQ"] = "sceNpCheckNpAvailability";
    m_names["eiqMCt9UshI"] = "sceNpCreateAsyncRequest";
    m_names["a8R9-75u4iM"] = "sceNpGetAccountId";
    m_names["p-o74CnoNzY"] = "sceNpGetNpId";
    m_names["ilwLM4zOmu4"] = "sceNpGetParentalControlInfo";
    m_names["eQH7nWPcAgc"] = "sceNpGetState";
    m_names["F6E4ycq9Dbg"] = "sceNpGetUserIdByOnlineId";
    m_names["s4UEa5iBJdc"] = "sceNpInGameMessageCreateHandle";
    m_names["+anuSx2avHQ"] = "sceNpInGameMessageDeleteHandle";
    m_names["GFhVUpRmbHE"] = "sceNpInGameMessageInitialize";
    m_names["Vh1bhUG6mSs"] = "sceNpInGameMessagePrepare";
    m_names["ON7Sf5XEMmI"] = "sceNpInGameMessageSendData";
    m_names["bMG3cVmUmuk"] = "sceNpInGameMessageTerminate";
    m_names["uqcPJLWL08M"] = "sceNpPollAsync";
    m_names["hw5KNqAAels"] = "sceNpRegisterNpReachabilityStateCallback";
    m_names["0c7HbXRKUt4"] = "sceNpRegisterStateCallbackForToolkit";
    m_names["A2CQ3kgSopQ"] = "sceNpSetContentRestriction";
    m_names["Ec63y59l9tw"] = "sceNpSetNpTitleId";
    m_names["cRILAEvn+9M"] = "sceNpUnregisterNpReachabilityStateCallback";
    m_names["xViqJdDgKl0"] = "sceNpUnregisterPlusEventCallback";
    m_names["YIvqqvJyjEc"] = "sceNpUnregisterStateCallbackForToolkit";
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
    m_names["i-XwZjw0OOY"] = "_ZN3sce4Json12MemAllocator11notifyErrorEimPv";
    m_names["-hJRce8wn1U"] = "_ZN3sce4Json12MemAllocatorC2Ev";
    m_names["OcAgPxcq5Vk"] = "_ZN3sce4Json12MemAllocatorD2Ev";
    m_names["zQtLRTqceMY"] = "_ZN3sce4Json5Array9push_backERKNS0_5ValueE";
    m_names["JP-PtKMiI1E"] = "_ZN3sce4Json5ArrayC1Ev";
    m_names["HJ8GpRT1aiw"] = "_ZN3sce4Json5ArrayD1Ev";
    m_names["R7FDWtcN6f8"] = "_ZN3sce4Json5Value9serializeERNS0_6StringE";
    m_names["iZeYfOxtMRg"] = "_ZN3sce4Json5ValueC1ERKNS0_5ArrayE";
    m_names["3xUXnmUkXfo"] = "_ZN3sce4Json5ValueC1ERKNS0_6ObjectE";
    m_names["sZIoMRGO+jk"] = "_ZN3sce4Json5ValueC1ERKNS0_6StringE";
    m_names["a+W7HHlwpBs"] = "_ZN3sce4Json6ObjectC1ERKS1_";
    m_names["OJPTonqdg0I"] = "_ZN3sce4Json6ObjectC1Ev";
    m_names["5JmzZt8twAo"] = "_ZN3sce4Json6ObjectD1Ev";
    m_names["urOpESTBZmo"] = "_ZN3sce4Json6ObjectaSERKS1_";
    m_names["ERuf9y0DY84"] = "_ZN3sce4Json6ObjectixERKNS0_6StringE";
    m_names["9KUZFjI1IxA"] = "_ZN3sce4Json6StringC1EPKc";
    m_names["qSmqLXXCPas"] = "_ZN3sce4Json6StringC1Ev";
    m_names["cG1VE2HMl6c"] = "_ZN3sce4Json6StringD1Ev";
    m_names["rQGJeNjOuUk"] = "_ZNK3sce4Json5Array4sizeEv";
    m_names["L1KAkYWml-M"] = "_ZNK3sce4Json6String5c_strEv";

    // ===== libSceHttp.sprx.json (19 symbols) =====
    m_names["mSQCxzWTwVI"] = "sceHttpsDisableOption";
    m_names["OXXX4mUk3uk"] = "sceNetConnect";
    m_names["Z4wwCFiBELQ"] = "sceNetCtlTerm";
    m_names["w21YgGGNtBk"] = "sceNetEpollAbort";
    m_names["ZVw46bsasAk"] = "sceNetEpollControl";
    m_names["SF47kB2MNTo"] = "sceNetEpollCreate";
    m_names["Inp1lfL+Jdw"] = "sceNetEpollDestroy";
    m_names["drjIbDbA7UQ"] = "sceNetEpollWait";
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
    m_names["07O525HgICs"] = "sceRtcTickAddSeconds";

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
    m_names["KMcEa+rHsIo"] = "sceAvPlayerAddSource";
    m_names["NkJwDzKmIlw"] = "sceAvPlayerClose";
    m_names["ODJK2sn9w4A"] = "sceAvPlayerEnableStream";
    m_names["d8FcbzfAdQw"] = "sceAvPlayerGetStreamInfo";
    m_names["JdksQu8pNdQ"] = "sceAvPlayerGetVideoDataEx";
    m_names["aS66RI0gGgo"] = "sceAvPlayerInit";
    m_names["UbQoYawOsfY"] = "sceAvPlayerIsActive";
    m_names["9y5v+fGN4Wk"] = "sceAvPlayerPause";
    m_names["HD1YKVU26-M"] = "sceAvPlayerPostInit";
    m_names["w5moABNwnRY"] = "sceAvPlayerResume";
    m_names["OVths0xGfho"] = "sceAvPlayerSetLooping";
    m_names["ET4Gr-Uu07s"] = "sceAvPlayerStart";
    m_names["ZC17w3vB5Lo"] = "sceAvPlayerStop";
    m_names["hdTyRzCXQeQ"] = "sceAvPlayerStreamCount";

    // ===== libSceNpTrophy.sprx.json (13 symbols) =====
    m_names["cqGkYAN-gRw"] = "sceNpTrophyCaptureScreenshot";
    m_names["XbkjbobZlCY"] = "sceNpTrophyCreateContext";
    m_names["q7U6tEAQf7c"] = "sceNpTrophyCreateHandle";
    m_names["E1Wrwd07Lr8"] = "sceNpTrophyDestroyContext";
    m_names["GNcF4oidY0Y"] = "sceNpTrophyDestroyHandle";
    m_names["HLwz1fRIycA"] = "sceNpTrophyGetGameIcon";
    m_names["YYP3f2W09og"] = "sceNpTrophyGetGameInfo";
    m_names["w4uMPmErD4I"] = "sceNpTrophyGetGroupIcon";
    m_names["wTUwGfspKic"] = "sceNpTrophyGetGroupInfo";
    m_names["eBL+l6HG9xk"] = "sceNpTrophyGetTrophyIcon";
    m_names["qqUVGDgQBm0"] = "sceNpTrophyGetTrophyInfo";
    m_names["TJCAxto9SEU"] = "sceNpTrophyRegisterContext";
    m_names["28xmRUFao68"] = "sceNpTrophyUnlockTrophy";

    // ===== libSceJson.sprx.json (12 symbols) =====
    m_names["Cxwy7wHq4J0"] = "_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE";
    m_names["PR5k1penBLM"] = "_ZN3sce4Json11Initializer9terminateEv";
    m_names["cK6bYHf-Q5E"] = "_ZN3sce4Json11InitializerC1Ev";
    m_names["RujUxbr3haM"] = "_ZN3sce4Json11InitializerD1Ev";
    m_names["XL8+BUqjB1w"] = "_ZN3sce4Json5Value3setERKS1_";
    m_names["FIjXN2TkuTs"] = "_ZN3sce4Json5Value5clearEv";
    m_names["sOmU4vnx3s0"] = "_ZN3sce4Json5ValueC1Ed";
    m_names["0lLK8+kDqmE"] = "_ZN3sce4Json5ValueC1El";
    m_names["x4AUdbhpRB0"] = "_ZN3sce4Json5ValueC1Em";
    m_names["qBMjqyBn3OM"] = "_ZN3sce4Json5ValueC1Ev";
    m_names["WTtYf+cNnXI"] = "_ZN3sce4Json5ValueD1Ev";
    m_names["4zrm6VrgIAw"] = "_ZN3sce4Json5ValueaSERKS1_";

    // ===== libSceAudio3d.sprx.json (12 symbols) =====
    m_names["Im+jOoa5WAI"] = "sceAudio3dGetDefaultOpenParameters";
    m_names["UmCvjSmuZIw"] = "sceAudio3dInitialize";
    m_names["jO2tec4dJ2M"] = "sceAudio3dObjectReserve";
    m_names["4uyHN9q4ZeU"] = "sceAudio3dObjectSetAttributes";
    m_names["1HXxo-+1qCw"] = "sceAudio3dObjectUnreserve";
    m_names["lw0qrdSjZt8"] = "sceAudio3dPortAdvance";
    m_names["OyVqOeVNtSk"] = "sceAudio3dPortClose";
    m_names["YaaDbDwKpFM"] = "sceAudio3dPortGetQueueLevel";
    m_names["XeDDK0xJWQA"] = "sceAudio3dPortOpen";
    m_names["VEVhZ9qd4ZY"] = "sceAudio3dPortPush";
    m_names["Yq9bfUQ0uJg"] = "sceAudio3dPortSetAttribute";
    m_names["WW1TS2iz5yc"] = "sceAudio3dTerminate";

    // ===== libSceSaveData.sprx.json (9 symbols) =====
    m_names["S1GkePI17zQ"] = "sceSaveDataDelete";
    m_names["dyIhnXq-0SM"] = "sceSaveDataDirNameSearch";
    m_names["TywrFKCoLGY"] = "sceSaveDataInitialize3";
    m_names["0z45PIH+SNI"] = "sceSaveDataMount2";
    m_names["c88Yy54Mx0w"] = "sceSaveDataSaveIcon";
    m_names["85zul--eGXs"] = "sceSaveDataSetParam";
    m_names["yKDy8S5yLA0"] = "sceSaveDataTerminate";
    m_names["WAzWTZm1H+I"] = "sceSaveDataTransferringMount";
    m_names["BMR4F-Uek3E"] = "sceSaveDataUmount";

    // ===== libSceContentSearch.sprx.json (9 symbols) =====
    m_names["-YbpaF0XS-I"] = "sceContentSearchCloseMetadata";
    m_names["ruNe-FgCzO8"] = "sceContentSearchGetMetadataValue";
    m_names["FRT4EYtZU1Y"] = "sceContentSearchGetMyApplicationIndex";
    m_names["o-RBPV0qr8c"] = "sceContentSearchGetNumOfContent";
    m_names["dPj4ZtRcIWk"] = "sceContentSearchInit";
    m_names["bjAlYWwRTJA"] = "sceContentSearchOpenMetadataByContentId";
    m_names["TEW3IKxYfXc"] = "sceContentSearchSearchContent";
    m_names["1xSZodB2geA"] = "sceContentSearchTerm";
    m_names["CdWp0oHWGr0"] = "sceUserServiceGetInitialUser";

    // ===== libSceContentExport.sprx.json (9 symbols) =====
    m_names["tb3cZTCl8Ps"] = "sceContentExportFinish";
    m_names["AOWqIYsgVHs"] = "sceContentExportFromData";
    m_names["uZTQHI50WpY"] = "sceContentExportFromDataWithThumbnail";
    m_names["FzEWeYnAFlI"] = "sceContentExportInit";
    m_names["FCygF4Ec4so"] = "sceContentExportStart";
    m_names["+KDWny9Y-6k"] = "sceContentExportTerm";
    m_names["fPhymKNvK-A"] = "sceUserServiceGetLoginUserIdList";
    m_names["j3YMu1MVNNo"] = "sceUserServiceInitialize";
    m_names["bwFjS+bX9mA"] = "sceUserServiceTerminate";

    // ===== libSceSaveDataDialog.sprx.json (8 symbols) =====
    m_names["fH46Lag88XY"] = "sceSaveDataDialogClose";
    m_names["yEiJ-qqr6Cg"] = "sceSaveDataDialogGetResult";
    m_names["ERKzksauAJA"] = "sceSaveDataDialogGetStatus";
    m_names["s9e3+YpRnzw"] = "sceSaveDataDialogInitialize";
    m_names["4tPhsP6FpDI"] = "sceSaveDataDialogOpen";
    m_names["hay1CfTmLyA"] = "sceSaveDataDialogProgressBarSetValue";
    m_names["YuH2FA7azqQ"] = "sceSaveDataDialogTerminate";
    m_names["KK3Bdg1RWK0"] = "sceSaveDataDialogUpdateStatus";

    // ===== libSceNet.sprx.json (8 symbols) =====
    m_names["PIWqhn9oSxc"] = "sceNetAccept";
    m_names["bErx49PgxyY"] = "sceNetBind";
    m_names["6Oc0bLsIYe0"] = "sceNetGetMacAddress";
    m_names["TCkRD0DWNLg"] = "sceNetGetpeername";
    m_names["hoOAofhhRvE"] = "sceNetGetsockname";
    m_names["kOj1HiAGE54"] = "sceNetListen";
    m_names["RCCY01Xd+58"] = "sceNetResolverStartNtoaMultipleRecords";
    m_names["TSM6whtekok"] = "sceNetShutdown";

    // ===== libSceSsl.sprx.json (7 symbols) =====
    m_names["RwXD8grHZHM"] = "sceSslFreeSslCertName";
    m_names["7whYpYfHP74"] = "sceSslGetIssuerName";
    m_names["R1ePzopYPYM"] = "sceSslGetNameEntryCount";
    m_names["7RBSTKGrmDA"] = "sceSslGetNameEntryInfo";
    m_names["xHpt6+2pGYk"] = "sceSslGetNotAfter";
    m_names["Eo0S65Jy28Q"] = "sceSslGetNotBefore";
    m_names["dQReuBX9sD8"] = "sceSslGetSubjectName";

    // ===== libSceAppContent.sprx.json (6 symbols) =====
    m_names["99b82IKXpH4"] = "sceAppContentAppParamGetInt";
    m_names["xnd8BJzAxmk"] = "sceAppContentGetAddcontInfoList";
    m_names["XTWR0UXvcgs"] = "sceAppContentGetEntitlementKey";
    m_names["R9lA82OraNs"] = "sceAppContentInitialize";
    m_names["buYbeLOGWmA"] = "sceAppContentTemporaryDataMount2";
    m_names["bcolXMmp6qQ"] = "sceAppContentTemporaryDataUnmount";

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
    m_names["XDncXQIJUSk"] = "sceNpGetOnlineId";

    // ===== libScePad.sprx.json (6 symbols) =====
    RegisterSymbol("6ncge5+l5Qs", reinterpret_cast<uintptr_t>(&Pad::scePadClose), "scePadClose");
    m_names["gjP9-KQzoUk"] = "scePadGetControllerInformation";
    m_names["rIZnR6eSpvk"] = "scePadResetOrientation";
    RegisterSymbol("RR4novUEENY", reinterpret_cast<uintptr_t>(&Pad::scePadSetLightBar), "scePadSetLightBar");
    m_names["vDLMoJLde8I"] = "scePadSetTiltCorrectionState";
    RegisterSymbol("yFVnOdGxvZY", reinterpret_cast<uintptr_t>(&Pad::scePadSetVibration), "scePadSetVibration");

    // ===== libSceMsgDialog.sprx.json (6 symbols) =====
    m_names["Lr8ovHH9l6A"] = "sceMsgDialogGetResult";
    m_names["CWVW78Qc3fI"] = "sceMsgDialogGetStatus";
    m_names["lDqxaY1UbEo"] = "sceMsgDialogInitialize";
    m_names["b06Hh0DPEaE"] = "sceMsgDialogOpen";
    m_names["ePw-kqZmelo"] = "sceMsgDialogTerminate";
    m_names["6fIC3XKt2k0"] = "sceMsgDialogUpdateStatus";

    // ===== libSceFont.sprx.json (6 symbols) =====
    m_names["L97d+3OgMlE"] = "sceFontGetCharGlyphMetrics";
    m_names["BozJej5T6fs"] = "sceFontGetPixelResolution";
    m_names["ryPlnDDI3rU"] = "sceFontGetRenderScaledKerning";
    m_names["KXUpebrFk1U"] = "sceFontOpenFontMemory";
    m_names["kAenWy1Zw5o"] = "sceFontRenderCharGlyphImageHorizontal";
    m_names["N1EBMeGhf7E"] = "sceFontSetScalePixel";

    // ===== libSceSigninDialog.sprx.json (6 symbols) =====
    m_names["M3OkENHcyiU"] = "sceSigninDialogClose";
    m_names["nqG7rqnYw1U"] = "sceSigninDialogGetResult";
    m_names["mlYGfmqE3fQ"] = "sceSigninDialogInitialize";
    m_names["JlpJVoRWv7U"] = "sceSigninDialogOpen";
    m_names["LXlmS6PvJdU"] = "sceSigninDialogTerminate";
    m_names["Bw31liTFT3A"] = "sceSigninDialogUpdateStatus";

    // ===== libSceFiber.sprx.json (5 symbols) =====
    m_names["hVYD7Ou2pCQ"] = "_sceFiberInitializeImpl";
    m_names["JeNX5F-NzQU"] = "sceFiberFinalize";
    m_names["B0ZX2hx9DMw"] = "sceFiberReturnToThread";
    m_names["a0LLrZWac0M"] = "sceFiberRun";
    m_names["PFT2S-tJ7Uk"] = "sceFiberSwitch";

    // ===== libScePlayGoDialog.sprx.json (5 symbols) =====
    m_names["fbigNQiZpm0"] = "scePlayGoDialogClose";
    m_names["fECamTJKpsM"] = "scePlayGoDialogInitialize";
    m_names["kHd72ukqbxw"] = "scePlayGoDialogOpen";
    m_names["okgIGdr5Iz0"] = "scePlayGoDialogTerminate";
    m_names["Yb60K7BST48"] = "scePlayGoDialogUpdateStatus";

    // ===== libSceNpCommon.sprx.json (5 symbols) =====
    m_names["i8UmXTSq7N4"] = "sceNpCmpNpId";
    m_names["dj+O5aD2a0Q"] = "sceNpCmpOnlineId";
    m_names["NR1J0N7L2xY"] = "sceRtcTickAddDays";
    m_names["MDc5cd8HfCA"] = "sceRtcTickAddHours";
    m_names["mn-tf4QiFzk"] = "sceRtcTickAddMinutes";

    // ===== libScePngDec.sprx.json (5 symbols) =====
    m_names["m0uW+8pFyaw"] = "scePngDecCreate";
    m_names["WC216DD3El4"] = "scePngDecDecode";
    m_names["QbD+eENEwo8"] = "scePngDecDelete";
    m_names["U6h4e5JRPaQ"] = "scePngDecParseHeader";
    m_names["-6srIGbLTIU"] = "scePngDecQueryMemorySize";

    // ===== libSceJpegDec.sprx.json (5 symbols) =====
    m_names["JPh3Zgg0Zwc"] = "sceJpegDecCreate";
    m_names["1kzQRoWEgSA"] = "sceJpegDecDecode";
    m_names["Hwh11+m5KoI"] = "sceJpegDecDelete";
    m_names["LSinoSQH790"] = "sceJpegDecParseHeader";
    m_names["uNAUmANZMEw"] = "sceJpegDecQueryMemorySize";

    // ===== libSceErrorDialog.sprx.json (5 symbols) =====
    m_names["t2FvHRXzgqk"] = "sceErrorDialogGetStatus";
    m_names["I88KChlynSs"] = "sceErrorDialogInitialize";
    m_names["M2ZF-ClLhgY"] = "sceErrorDialogOpen";
    m_names["9XAxK2PMwk8"] = "sceErrorDialogTerminate";
    m_names["WWiGuh9XfgQ"] = "sceErrorDialogUpdateStatus";

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
    m_names["WJ3rqFwymew"] = "sceRtcFormatRFC3339";
    m_names["18B2NS1y9UU"] = "sceRtcGetCurrentTick";
    m_names["jMNwqYr4R-k"] = "sceRtcGetTickResolution";
    m_names["99bMGglFW3I"] = "sceRtcParseRFC3339";

    // ===== libSceRtc.sprx.json (4 symbols) =====
    m_names["eiuobaF-hK4"] = "sceRtcFormatRFC2822";
    m_names["AxHBk3eat04"] = "sceRtcFormatRFC2822LocalTime";
    m_names["CL6y9q-XbuQ"] = "sceRtcTickAddMonths";
    m_names["-5y2uJ62qS8"] = "sceRtcTickAddYears";

    // ===== libSceNpEulaDialog.sprx.json (4 symbols) =====
    m_names["GpLQDNKICac"] = "sceNpCreateRequest";
    m_names["S7QTn72PrDw"] = "sceNpDeleteRequest";
    m_names["Ghz9iWDUtC4"] = "sceNpGetAccountCountry";
    m_names["KZ1Mj9yEGYc"] = "sceNpGetAccountLanguage";

    // ===== libSceShareUtility.sprx.json (4 symbols) =====
    m_names["Jlv-lYxEnvM"] = "sceShareUtilityInitialize";
    m_names["Lqp4EDIRXSo"] = "sceShareUtilityOpenShareMenu";
    m_names["8hZ2EEl2Tto"] = "sceShareUtilityOpenShareMenuDefault";
    m_names["DUWhxkyVPj4"] = "sceShareUtilityTerminate";

    // ===== libSceVideoCoreInterface.sprx.json (4 symbols) =====
    m_names["HXzjK9yI30k"] = "sceVideoOutAddFlipEvent";
    RegisterSymbol("Up36PTk687E", reinterpret_cast<uintptr_t>(&Video::sceVideoOutOpen), "sceVideoOutOpen");
    m_names["w3BY+tAEiQY"] = "sceVideoOutRegisterBuffers";
    m_names["i6-sR91Wt-4"] = "sceVideoOutSetBufferAttribute";

    // ===== libSceAbstractTwitter.sprx.json (3 symbols) =====
    m_names["zO9UL3qIINQ"] = "sceRtcGetCurrentNetworkTick";
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
    m_names["wVwPU50pS1c"] = "sceAudioOutSetMixLevelPadSpk";

    // ===== libSceNpAuth.sprx.json (3 symbols) =====
    m_names["6bwFkosYRQg"] = "sceNpAuthCreateRequest";
    m_names["H8wG9Bk-nPc"] = "sceNpAuthDeleteRequest";
    m_names["KxGkOrQJTqY"] = "sceNpAuthGetAuthorizationCode";

    // ===== libSceNpSns.sprx.json (3 symbols) =====
    m_names["OoTjfxl8-wI"] = "sceNpSnsFacebookCreateRequest";
    m_names["pTvAKV1iQkE"] = "sceNpSnsFacebookDeleteRequest";
    m_names["VtyS8XLBqNE"] = "sceNpSnsFacebookGetAccessToken";

    // ===== libSceSystemService.sprx.json (2 symbols) =====
    m_names["Vo5V8KAwCmk"] = "sceSystemServiceHideSplashScreen";
    m_names["656LMQSrg6U"] = "sceSystemServiceReceiveEvent";

    // ===== libSceAbstractDailymotion.sprx.json (2 symbols) =====
    m_names["YuOW3dDAKYc"] = "sceHttpUriEscape";
    m_names["8w-H19ip48I"] = "sceRtcGetTick";

    // ===== libSceGvMp4Parser.sprx.json (2 symbols) =====
    m_names["8Yr143yEnRo"] = "sceRtcConvertLocalTimeToUtc";
    m_names["M1TvFst-jrM"] = "sceRtcConvertUtcToLocalTime";

    // ===== libSceOrbisCompat.sprx.json (2 symbols) =====
    RegisterSymbol("xk0AcarP3V4", reinterpret_cast<uintptr_t>(&Pad::scePadOpen), "scePadOpen");
    RegisterSymbol("YndgXqQVV7c", reinterpret_cast<uintptr_t>(&Pad::scePadReadState), "scePadReadState");

    // ===== libSceAvPlayerStreaming.sprx.json (2 symbols) =====
    m_names["qFg2SuyTJJY"] = "sceHttpSetAuthEnabled";
    m_names["NxEI1KByvCI"] = "sceRtcParseDateTime";

    // ===== libSceUserService.sprx.json (2 symbols) =====
    m_names["yH17Q6NWtVg"] = "sceUserServiceGetEvent";
    m_names["1xxcMiGu2fo"] = "sceUserServiceGetUserName";

    // ===== libSceNgs2.sprx.json (2 symbols) =====
    m_names["cLV4aiT9JpA"] = "sceNgs2RackCreate";
    m_names["0eFLVCfWVds"] = "sceNgs2RackQueryBufferSize";

    // ===== libmonosgen-2.0.sprx.json (2 symbols) =====
    m_names["HQOwnfMGipQ"] = "sceNetErrnoLoc";
    m_names["C4UgDHHPvdw"] = "sceNetResolverCreate";

    // ===== libSceCompanionHttpd.sprx.json (2 symbols) =====
    m_names["obuxdTiwkF8"] = "sceNetCtlGetInfo";
    m_names["gky0+oaNM4k"] = "sceNetCtlInit";

    // ===== libSceCommonDialog.sprx.json (1 symbols) =====
    m_names["uoUpLGNkygk"] = "sceCommonDialogInitialize";

    // ===== libSceGameLiveStreaming.sprx.json (1 symbols) =====
    m_names["rPo6tV8D9bM"] = "sceSystemServiceGetStatus";

    // ===== libSceOrbisCompatForVideoService.sprx.json (1 symbols) =====
    m_names["hv1luiJrqQM"] = "scePadInit";

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
    m_names["JO4yuTuMoKI"] = "sceNetCtlGetNatInfo";

    LOG_INFO("NID", "Registered " + std::to_string(m_symbols.size()) + " native host API symbols and " +
             std::to_string(m_names.size()) + " named API stubs.");
}

} // namespace Loader
