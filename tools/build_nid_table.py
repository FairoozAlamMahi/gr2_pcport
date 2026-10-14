import re

# Read declared functions in libkernel.h
with open('src/kernel/libkernel.h', 'r') as f:
    hdr = f.read()

kernel_funcs = set(re.findall(r'PS4_SYSV_ABI\s+(?:[\w\*]+)\s+(\w+)\s*\(', hdr))
print(f'Detected {len(kernel_funcs)} functions in libkernel.h')

# Aliases: name in ps4libdoc -> function in Kernel namespace
aliases = {
    'pthread_attr_destroy': 'scePthreadAttrDestroy',
    'pthread_attr_init': 'scePthreadAttrInit',
    'pthread_attr_setdetachstate': 'scePthreadAttrSetdetachstate',
    'pthread_attr_setstacksize': 'scePthreadAttrSetstacksize',
    'pthread_create_name_np': 'scePthreadCreate',
    'pthread_join': 'scePthreadJoin',
    'pthread_mutex_destroy': 'scePthreadMutexDestroy',
    'pthread_mutex_init': 'scePthreadMutexInit',
    'pthread_mutex_lock': 'scePthreadMutexLock',
    'pthread_mutex_trylock': 'scePthreadMutexTrylock',
    'pthread_mutex_unlock': 'scePthreadMutexUnlock',
    'pthread_mutexattr_destroy': 'scePthreadMutexattrDestroy',
    'pthread_mutexattr_init': 'scePthreadMutexattrInit',
    'pthread_mutexattr_settype': 'scePthreadMutexattrSettype',
    'pthread_self': 'scePthreadSelf',
    'sched_yield': 'scePthreadYield',
    'sceKernelDebugRaiseExceptionOnReleaseMode': 'sceKernelDebugRaiseException',
    'nanosleep': 'ps4_nanosleep',
    '_exit': 'ps4_exit',
    'socket': 'ps4_socket',
    'bind': 'ps4_bind',
    'listen': 'ps4_listen',
    'accept': 'ps4_accept',
    'send': 'ps4_send',
    'recv': 'ps4_recv',
    'shutdown': 'ps4_shutdown',
    'select': 'ps4_select',
    'close': 'sceKernelClose',
    'rename': 'sceKernelRename',
    'clock_gettime': 'sceKernelClockGettime',
    'gettimeofday': 'sceKernelGettimeofday',
    'signal': 'ps4_signal',
    '_read': 'ps4_read',
    '_write': 'ps4_write',
}

# Read generated_host_nids.cpp
with open('tools/generated_host_nids.cpp', 'r') as f:
    lines = f.readlines()

output_lines = []
output_lines.append('''#include "loader/nid_table.h"
#include "common/logger.h"
#include "kernel/libkernel.h"
#include "pad/pad_handler.h"
#include "video/video_out.h"
#include "audio/audio_out.h"
#include "gpu/gnm_driver.h"
#include <iostream>
#include <sstream>
#include <iomanip>

extern "C" void HostStubLogger(u64 stub_id, uintptr_t caller_rip) {
    char buf[512];
    const std::string& name = Loader::NidTable::Instance().GetStubName(stub_id);
    int len = snprintf(buf, sizeof(buf), "[WARN ][STUB] Unhandled guest API called: [%s] from 0x%llX\\n",
                       name.c_str(), static_cast<unsigned long long>(caller_rip));
    if (len > 0) {
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(len), &written, nullptr);
    }
}

__attribute__((naked)) static void CommonStubEntry() {
    asm volatile(
        "pushq %rdi\\n\\t"
        "pushq %rsi\\n\\t"
        "pushq %rdx\\n\\t"
        "pushq %rcx\\n\\t"
        "pushq %r8\\n\\t"
        "pushq %r9\\n\\t"
        "pushq %r10\\n\\t"
        "pushq %r11\\n\\t"
        "movq %rax, %rcx\\n\\t"
        "movq 64(%rsp), %rdx\\n\\t"
        "subq $40, %rsp\\n\\t"
        "callq HostStubLogger\\n\\t"
        "addq $40, %rsp\\n\\t"
        "popq %r11\\n\\t"
        "popq %r10\\n\\t"
        "popq %r9\\n\\t"
        "popq %r8\\n\\t"
        "popq %rcx\\n\\t"
        "popq %rdx\\n\\t"
        "popq %rsi\\n\\t"
        "popq %rdi\\n\\t"
        "xorq %rax, %rax\\n\\t"
        "retq\\n\\t"
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
''')

# Process lines from generated_host_nids.cpp
cur_cat = ""
registered_count = 0
named_stubs = 0

for line in lines:
    line_s = line.strip()
    if line_s.startswith("// ====="):
        output_lines.append(f"\n    {line_s}")
        cur_cat = line_s
    elif line_s.startswith("RegisterSymbol("):
        parts = line_s.split('"')
        if len(parts) >= 4:
            nid = parts[1]
            fn_name = parts[3]
            
            # Check if Kernel implementation exists
            target_fn = None
            if fn_name in kernel_funcs:
                target_fn = f"&Kernel::{fn_name}"
            elif fn_name in aliases and aliases[fn_name] in kernel_funcs:
                target_fn = f"&Kernel::{aliases[fn_name]}"
            
            # Check Pad
            if fn_name == 'scePadOpen': target_fn = '&Pad::scePadOpen'
            elif fn_name == 'scePadClose': target_fn = '&Pad::scePadClose'
            elif fn_name == 'scePadReadState': target_fn = '&Pad::scePadReadState'
            elif fn_name == 'scePadSetVibration': target_fn = '&Pad::scePadSetVibration'
            elif fn_name == 'scePadSetLightBar': target_fn = '&Pad::scePadSetLightBar'

            # Check Video
            if fn_name == 'sceVideoOutOpen': target_fn = '&Video::sceVideoOutOpen'
            elif fn_name == 'sceVideoOutClose': target_fn = '&Video::sceVideoOutClose'
            elif fn_name == 'sceVideoOutSubmitFlip': target_fn = '&Video::sceVideoOutSubmitFlip'
            elif fn_name == 'sceVideoOutSetFlipRate': target_fn = '&Video::sceVideoOutSetFlipRate'
            elif fn_name == 'sceVideoOutGetVblankStatus': target_fn = '&Video::sceVideoOutGetVblankStatus'

            # Check Audio
            if fn_name == 'sceAudioOutOpen': target_fn = '&Audio::sceAudioOutOpen'
            elif fn_name == 'sceAudioOutClose': target_fn = '&Audio::sceAudioOutClose'
            elif fn_name == 'sceAudioOutOutput': target_fn = '&Audio::sceAudioOutOutput'
            elif fn_name == 'sceAudioOutSetVolume': target_fn = '&Audio::sceAudioOutSetVolume'
            elif fn_name == 'sceAudioOutGetPortTimestamp': target_fn = '&Audio::sceAudioOutGetPortTimestamp'

            # Check GnmDriver
            if fn_name == 'sceGnmSubmitCommandBuffers': target_fn = '&GPU::sceGnmSubmitCommandBuffers'
            elif fn_name == 'sceGnmSubmitDone': target_fn = '&GPU::sceGnmSubmitDone'
            elif fn_name == 'sceGnmDrawInitDefaultHardwareState': target_fn = '&GPU::sceGnmDrawInitDefaultHardwareState'
            elif fn_name == 'sceGnmInsertPushMarker': target_fn = '&GPU::sceGnmInsertPushMarker'
            elif fn_name == 'sceGnmInsertPopMarker': target_fn = '&GPU::sceGnmInsertPopMarker'
            elif fn_name == 'sceGnmFlushGarlic': target_fn = '&GPU::sceGnmFlushGarlic'
            elif fn_name == 'sceGnmSetCsShader': target_fn = '&GPU::sceGnmSetCsShader'
            elif fn_name == 'sceGnmSetPsShader': target_fn = '&GPU::sceGnmSetPsShader'
            elif fn_name == 'sceGnmSetVsShader': target_fn = '&GPU::sceGnmSetVsShader'
            elif fn_name == 'sceGnmDispatchDirect': target_fn = '&GPU::sceGnmDispatchDirect'

            if target_fn:
                output_lines.append(f'    RegisterSymbol("{nid}", reinterpret_cast<uintptr_t>({target_fn}), "{fn_name}");')
                registered_count += 1
            else:
                # Pre-register name so stub displays real API name
                output_lines.append(f'    m_names["{nid}"] = "{fn_name}";')
                named_stubs += 1

output_lines.append('''
    LOG_INFO("NID", "Registered " + std::to_string(m_symbols.size()) + " native host API symbols and " +
             std::to_string(m_names.size()) + " named API stubs.");
}

} // namespace Loader
''')

final_code = "\n".join(output_lines)
with open('src/loader/nid_table.cpp', 'w', encoding='utf-8') as out:
    out.write(final_code)

print(f"Generated nid_table.cpp with {registered_count} real implementations and {named_stubs} named stubs!")
