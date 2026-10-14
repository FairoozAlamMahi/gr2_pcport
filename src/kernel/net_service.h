#pragma once

#include "common/types.h"

namespace Net {

void Initialize();
void Shutdown();

extern "C" {

// libSceNetCtl
PS4_SYSV_ABI s32 sceNetCtlInit();
PS4_SYSV_ABI s32 sceNetCtlTerm();
PS4_SYSV_ABI s32 sceNetCtlGetState(s32* state);
PS4_SYSV_ABI s32 sceNetCtlGetInfo(u32 code, void* info);
PS4_SYSV_ABI s32 sceNetCtlGetNatInfo(void* nat_info);
PS4_SYSV_ABI s32 sceNetCtlCheckCallback();
PS4_SYSV_ABI s32 sceNetCtlRegisterCallback(const void* cb, void* arg, s32* cid);
PS4_SYSV_ABI s32 sceNetCtlUnregisterCallback(s32 cid);
PS4_SYSV_ABI s32 sceNetCtlGetResult(u32 code, s32* result);

// libSceNet
PS4_SYSV_ABI s32 sceNetInit();
PS4_SYSV_ABI s32 sceNetTerm();
PS4_SYSV_ABI s32 sceNetPoolCreate(const char* name, u32 size, u32 flags);
PS4_SYSV_ABI s32 sceNetPoolDestroy(s32 pool);
PS4_SYSV_ABI s32 sceNetGetMacAddress(void* mac, u32 flags);
PS4_SYSV_ABI s32 sceNetEpollCreate(const char* name, u32 flags);
PS4_SYSV_ABI s32 sceNetEpollControl(s32 epoll, s32 op, s32 fd, void* event);
PS4_SYSV_ABI s32 sceNetEpollWait(s32 epoll, void* events, s32 maxevents, s32 timeout);
PS4_SYSV_ABI s32 sceNetEpollDestroy(s32 epoll);
PS4_SYSV_ABI s32 sceNetEpollAbort(s32 epoll, u32 flags);
PS4_SYSV_ABI s32 sceNetResolverStartNtoaMultipleRecords(s32 rid, const char* name, void* addrs, u32* count, u32 flags, s32 timeout, void* reserved);
PS4_SYSV_ABI s32 sceNetShutdown(s32 s, s32 how);

} // extern "C"

} // namespace Net
