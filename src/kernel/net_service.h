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
PS4_SYSV_ABI s32 sceNetSocket(const char* name, s32 domain, s32 type, s32 protocol);
PS4_SYSV_ABI s32 sceNetSocketClose(s32 s);
PS4_SYSV_ABI s32 sceNetConnect(s32 s, const void* name, s32 namelen);
PS4_SYSV_ABI s32 sceNetSend(s32 s, const void* msg, usize len, s32 flags);
PS4_SYSV_ABI s32 sceNetRecv(s32 s, void* buf, usize len, s32 flags);
PS4_SYSV_ABI s32 sceNetBind(s32 s, const void* name, s32 namelen);
PS4_SYSV_ABI s32 sceNetListen(s32 s, s32 backlog);
PS4_SYSV_ABI s32 sceNetAccept(s32 s, void* addr, s32* addrlen);
PS4_SYSV_ABI s32 sceNetGetsockname(s32 s, void* name, s32* namelen);
PS4_SYSV_ABI s32 sceNetGetpeername(s32 s, void* name, s32* namelen);
PS4_SYSV_ABI s32 sceNetSetsockopt(s32 s, s32 level, s32 optname, const void* optval, s32 optlen);
PS4_SYSV_ABI u16 sceNetHtons(u16 host16);
PS4_SYSV_ABI u16 sceNetNtohs(u16 net16);
PS4_SYSV_ABI s32 sceNetInetPton(s32 af, const char* src, void* dst);
PS4_SYSV_ABI const char* sceNetInetNtop(s32 af, const void* src, char* dst, usize size);
PS4_SYSV_ABI s32* sceNetErrnoLoc();
PS4_SYSV_ABI s32 sceNetResolverCreate(const char* name, void* mem, usize mem_size);
PS4_SYSV_ABI s32 sceNetResolverAbort(s32 rid, u32 flags);
PS4_SYSV_ABI s32 sceNetResolverGetError(s32 rid, s32* error);

} // extern "C"

} // namespace Net
