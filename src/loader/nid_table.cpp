#include "loader/nid_table.h"
#include "common/logger.h"
#include "kernel/libkernel.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
#include "audio/audio_out.h"
#include "gpu/gnm_driver.h"
#include <iostream>

namespace Loader {

// Universal generic stub fallback for unhandled NIDs
static PS4_SYSV_ABI s64 GenericStubHandler() {
    return 0;
}

NidTable& NidTable::Instance() {
    static NidTable table;
    return table;
}

void NidTable::Initialize() {
    m_symbols.clear();
    m_names.clear();
    RegisterHostSubsystems();
}

void NidTable::RegisterSymbol(const std::string& nid_prefix, uintptr_t func_addr, const std::string& name) {
    m_symbols[nid_prefix] = func_addr;
    if (!name.empty()) {
        m_names[nid_prefix] = name;
    }
}

uintptr_t NidTable::Resolve(const std::string& nid_prefix, const std::string& full_symbol) const {
    auto it = m_symbols.find(nid_prefix);
    if (it != m_symbols.end()) {
        return it->second;
    }
    // Return universal fallback stub so unhooked minor APIs return cleanly (0)
    return reinterpret_cast<uintptr_t>(&GenericStubHandler);
}

void NidTable::RegisterHostSubsystems() {
    LOG_INFO("NID", "Registering Host System Subsystems into NID Symbol Registry...");

    // 1. libkernel
    RegisterSymbol("S3Et4iyZ0K0", reinterpret_cast<uintptr_t>(&Kernel::sceKernelOpen), "sceKernelOpen");
    RegisterSymbol("Ax940iArzkM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelClose), "sceKernelClose");
    RegisterSymbol("ukNBBv9WDXA", reinterpret_cast<uintptr_t>(&Kernel::sceKernelRead), "sceKernelRead");
    RegisterSymbol("3YtCsbxqPGM", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWrite), "sceKernelWrite");
    RegisterSymbol("kn0-k32rPJE", reinterpret_cast<uintptr_t>(&Kernel::sceKernelLseek), "sceKernelLseek");
    RegisterSymbol("MBYc6kmK11g", reinterpret_cast<uintptr_t>(&Kernel::sceKernelAllocateDirectMemory), "sceKernelAllocateDirectMemory");
    RegisterSymbol("61SRkUwIq4c", reinterpret_cast<uintptr_t>(&Kernel::sceKernelMapDirectMemory), "sceKernelMapDirectMemory");
    RegisterSymbol("M0m0BafMFfg", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetDirectMemorySize), "sceKernelGetDirectMemorySize");
    RegisterSymbol("pWFx2bo5OuQ", reinterpret_cast<uintptr_t>(&Kernel::sceKernelCreateSema), "sceKernelCreateSema");
    RegisterSymbol("OonUxX6jVHU", reinterpret_cast<uintptr_t>(&Kernel::sceKernelSignalSema), "sceKernelSignalSema");
    RegisterSymbol("x2S70omOz8k", reinterpret_cast<uintptr_t>(&Kernel::sceKernelWaitSema), "sceKernelWaitSema");
    RegisterSymbol("FObJjOlF5qo", reinterpret_cast<uintptr_t>(&Kernel::scePthreadCreate), "scePthreadCreate");
    RegisterSymbol("NMIpue9XcQk", reinterpret_cast<uintptr_t>(&Kernel::scePthreadJoin), "scePthreadJoin");
    RegisterSymbol("rDddMsCTYJA", reinterpret_cast<uintptr_t>(&Kernel::scePthreadSelf), "scePthreadSelf");
    RegisterSymbol("SW1qbFASHjQ", reinterpret_cast<uintptr_t>(&Kernel::scePthreadExit), "scePthreadExit");
    RegisterSymbol("xKF1H20+f3c", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcessTime), "sceKernelGetProcessTime");
    RegisterSymbol("QDTcvCqSzsw", reinterpret_cast<uintptr_t>(&Kernel::sceKernelGetProcessTimeCounter), "sceKernelGetProcessTimeCounter");

    // 2. libScePad
    RegisterSymbol("l4OVTpJCyQI", reinterpret_cast<uintptr_t>(&Pad::scePadOpen), "scePadOpen");
    RegisterSymbol("wj-U+mMbyAM", reinterpret_cast<uintptr_t>(&Pad::scePadClose), "scePadClose");
    RegisterSymbol("FdjoqFQOlt0", reinterpret_cast<uintptr_t>(&Pad::scePadReadState), "scePadReadState");
    RegisterSymbol("PbxGVfOvUQY", reinterpret_cast<uintptr_t>(&Pad::scePadSetVibration), "scePadSetVibration");
    RegisterSymbol("X+7rIfY97Ps", reinterpret_cast<uintptr_t>(&Pad::scePadSetLightBar), "scePadSetLightBar");

    // 3. libSceVideoOut
    RegisterSymbol("A2CQ3kgSopQ", reinterpret_cast<uintptr_t>(&Video::sceVideoOutOpen), "sceVideoOutOpen");
    RegisterSymbol("XDncXQIJUSk", reinterpret_cast<uintptr_t>(&Video::sceVideoOutClose), "sceVideoOutClose");
    RegisterSymbol("uqcPJLWL08M", reinterpret_cast<uintptr_t>(&Video::sceVideoOutSubmitFlip), "sceVideoOutSubmitFlip");
    RegisterSymbol("jyi5p9XWUSs", reinterpret_cast<uintptr_t>(&Video::sceVideoOutSetFlipRate), "sceVideoOutSetFlipRate");
    RegisterSymbol("2rsFmlGWleQ", reinterpret_cast<uintptr_t>(&Video::sceVideoOutGetVblankStatus), "sceVideoOutGetVblankStatus");

    // 4. libSceAudioOut
    RegisterSymbol("T8fER+tIGgk", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOpen), "sceAudioOutOpen");
    RegisterSymbol("fZOeZIOEmLw", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutClose), "sceAudioOutClose");
    RegisterSymbol("pxnCmagrtao", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutOutput), "sceAudioOutOutput");
    RegisterSymbol("Ez8xjo9UF4E", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutSetVolume), "sceAudioOutSetVolume");
    RegisterSymbol("TUuiYS2kE8s", reinterpret_cast<uintptr_t>(&Audio::sceAudioOutGetPortTimestamp), "sceAudioOutGetPortTimestamp");

    // 5. libSceGnmDriver
    RegisterSymbol("b0xyllnVY-I", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitCommandBuffers), "sceGnmSubmitCommandBuffers");
    RegisterSymbol("FUHG8sQ3R58", reinterpret_cast<uintptr_t>(&GPU::sceGnmSubmitDone), "sceGnmSubmitDone");
    RegisterSymbol("b08AgtPlHPg", reinterpret_cast<uintptr_t>(&GPU::sceGnmDrawInitDefaultHardwareState), "sceGnmDrawInitDefaultHardwareState");
    RegisterSymbol("ihxrbsoSKWc", reinterpret_cast<uintptr_t>(&GPU::sceGnmInsertPushMarker), "sceGnmInsertPushMarker");
    RegisterSymbol("5udAm+6boVg", reinterpret_cast<uintptr_t>(&GPU::sceGnmInsertPopMarker), "sceGnmInsertPopMarker");
    RegisterSymbol("qpGITzPE+Zc", reinterpret_cast<uintptr_t>(&GPU::sceGnmFlushGarlic), "sceGnmFlushGarlic");
    RegisterSymbol("jtkqXpAOY6w", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetCsShader), "sceGnmSetCsShader");
    RegisterSymbol("UJwNuMBcUAk", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetPsShader), "sceGnmSetPsShader");
    RegisterSymbol("VJNjFtqiF5w", reinterpret_cast<uintptr_t>(&GPU::sceGnmSetVsShader), "sceGnmSetVsShader");
    RegisterSymbol("vckdzbQ46SI", reinterpret_cast<uintptr_t>(&GPU::sceGnmDispatchDirect), "sceGnmDispatchDirect");

    LOG_INFO("NID", "Registered " + std::to_string(m_symbols.size()) + " native host API symbols.");
}

} // namespace Loader
