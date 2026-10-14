#include <winsock2.h>
#include <ws2tcpip.h>
#include "kernel/net_service.h"
#include "common/logger.h"
#include <cstring>
#include <atomic>
#include <string>

namespace Net {

static bool s_winsock_initialized = false;
static std::atomic<s32> s_next_pool_handle{1};
static std::atomic<s32> s_next_epoll_handle{1};

void Initialize() {
    if (!s_winsock_initialized) {
        WSADATA wsaData{};
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res == 0) {
            s_winsock_initialized = true;
            LOG_INFO("NET", "Winsock2 initialized successfully.");
        } else {
            LOG_WARN("NET", "WSAStartup failed with error: " + std::to_string(res));
        }
    }
}

void Shutdown() {
    if (s_winsock_initialized) {
        WSACleanup();
        s_winsock_initialized = false;
        LOG_INFO("NET", "Winsock2 shut down.");
    }
}

extern "C" {

// libSceNetCtl
PS4_SYSV_ABI s32 sceNetCtlInit() {
    Initialize();
    LOG_INFO("NET", "sceNetCtlInit called");
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlTerm() {
    LOG_INFO("NET", "sceNetCtlTerm called");
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlGetState(s32* state) {
    if (state) {
        *state = 2; // SCE_NET_CTL_STATE_IPOBTAINED (connected)
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlGetInfo(u32 code, void* info) {
    if (!info) return 0;
    std::memset(info, 0, 64);
    switch (code) {
    case 4: // IP_ADDRESS
        std::strncpy(reinterpret_cast<char*>(info), "192.168.1.100", 15);
        break;
    case 5: // NETMASK
        std::strncpy(reinterpret_cast<char*>(info), "255.255.255.0", 15);
        break;
    case 6: // DEFAULT_ROUTE
        std::strncpy(reinterpret_cast<char*>(info), "192.168.1.1", 15);
        break;
    case 7: // PRIMARY_DNS
        std::strncpy(reinterpret_cast<char*>(info), "8.8.8.8", 15);
        break;
    case 8: // SECONDARY_DNS
        std::strncpy(reinterpret_cast<char*>(info), "8.8.4.4", 15);
        break;
    case 9: { // ETHER_ADDR (MAC)
        static const u8 mac[6] = {0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13};
        std::memcpy(info, mac, 6);
        break;
    }
    default:
        *reinterpret_cast<u32*>(info) = 1;
        break;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlGetNatInfo(void* nat_info) {
    if (nat_info) {
        std::memset(nat_info, 0, 32);
        *reinterpret_cast<u32*>(nat_info) = 2; // NAT Type 2 (typical / moderate)
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlCheckCallback() {
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlRegisterCallback(const void* cb, void* arg, s32* cid) {
    (void)cb;
    (void)arg;
    if (cid) {
        *cid = 1;
    }
    LOG_INFO("NET", "sceNetCtlRegisterCallback registered (cid=1)");
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlUnregisterCallback(s32 cid) {
    (void)cid;
    return 0;
}

PS4_SYSV_ABI s32 sceNetCtlGetResult(u32 code, s32* result) {
    (void)code;
    if (result) {
        *result = 0; // SCE_OK
    }
    return 0;
}

// libSceNet
PS4_SYSV_ABI s32 sceNetInit() {
    Initialize();
    LOG_INFO("NET", "sceNetInit called");
    return 0;
}

PS4_SYSV_ABI s32 sceNetTerm() {
    LOG_INFO("NET", "sceNetTerm called");
    return 0;
}

PS4_SYSV_ABI s32 sceNetPoolCreate(const char* name, u32 size, u32 flags) {
    (void)name;
    (void)size;
    (void)flags;
    s32 pool = s_next_pool_handle++;
    LOG_INFO("NET", "sceNetPoolCreate created pool " + std::to_string(pool));
    return pool;
}

PS4_SYSV_ABI s32 sceNetPoolDestroy(s32 pool) {
    (void)pool;
    LOG_INFO("NET", "sceNetPoolDestroy called");
    return 0;
}

PS4_SYSV_ABI s32 sceNetGetMacAddress(void* mac, u32 flags) {
    (void)flags;
    if (mac) {
        static const u8 default_mac[6] = {0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13};
        std::memcpy(mac, default_mac, 6);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetEpollCreate(const char* name, u32 flags) {
    (void)name;
    (void)flags;
    s32 epoll = s_next_epoll_handle++;
    LOG_INFO("NET", "sceNetEpollCreate created epoll " + std::to_string(epoll));
    return epoll;
}

PS4_SYSV_ABI s32 sceNetEpollControl(s32 epoll, s32 op, s32 fd, void* event) {
    (void)epoll;
    (void)op;
    (void)fd;
    (void)event;
    return 0;
}

PS4_SYSV_ABI s32 sceNetEpollWait(s32 epoll, void* events, s32 maxevents, s32 timeout) {
    (void)epoll;
    (void)events;
    (void)maxevents;
    (void)timeout;
    return 0;
}

PS4_SYSV_ABI s32 sceNetEpollDestroy(s32 epoll) {
    (void)epoll;
    return 0;
}

PS4_SYSV_ABI s32 sceNetEpollAbort(s32 epoll, u32 flags) {
    (void)epoll;
    (void)flags;
    return 0;
}

PS4_SYSV_ABI s32 sceNetResolverStartNtoaMultipleRecords(s32 rid, const char* name, void* addrs, u32* count, u32 flags, s32 timeout, void* reserved) {
    (void)rid;
    (void)name;
    (void)addrs;
    (void)flags;
    (void)timeout;
    (void)reserved;
    if (count) {
        *count = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetShutdown(s32 s, s32 how) {
    (void)s;
    (void)how;
    return 0;
}

static thread_local s32 t_sce_net_errno = 0;
static std::atomic<s32> s_next_sock_fd{100};

PS4_SYSV_ABI s32* sceNetErrnoLoc() {
    return &t_sce_net_errno;
}

PS4_SYSV_ABI u16 sceNetHtons(u16 host16) {
    return (host16 << 8) | (host16 >> 8);
}

PS4_SYSV_ABI u16 sceNetNtohs(u16 net16) {
    return (net16 << 8) | (net16 >> 8);
}

PS4_SYSV_ABI s32 sceNetInetPton(s32 af, const char* src, void* dst) {
    if (!src || !dst) return -1;
    int family = (af == 2) ? AF_INET : AF_INET6;
    int res = inet_pton(family, src, dst);
    return res > 0 ? 1 : 0;
}

PS4_SYSV_ABI const char* sceNetInetNtop(s32 af, const void* src, char* dst, usize size) {
    if (!src || !dst || size == 0) return nullptr;
    int family = (af == 2) ? AF_INET : AF_INET6;
    return inet_ntop(family, src, dst, static_cast<socklen_t>(size));
}

PS4_SYSV_ABI s32 sceNetSocket(const char* name, s32 domain, s32 type, s32 protocol) {
    (void)name;
    (void)domain;
    (void)type;
    (void)protocol;
    s32 fd = s_next_sock_fd.fetch_add(1);
    LOG_INFO("NET", "sceNetSocket allocated fd " + std::to_string(fd));
    return fd;
}

PS4_SYSV_ABI s32 sceNetSocketClose(s32 s) {
    LOG_INFO("NET", "sceNetSocketClose fd " + std::to_string(s));
    return 0;
}

PS4_SYSV_ABI s32 sceNetConnect(s32 s, const void* name, s32 namelen) {
    (void)s;
    (void)name;
    (void)namelen;
    // In offline runner mode, report connection refused so the game's PSN client
    // cleanly falls back to offline mode without hanging.
    t_sce_net_errno = 61; // ECONNREFUSED
    return -1;
}

PS4_SYSV_ABI s32 sceNetSend(s32 s, const void* msg, usize len, s32 flags) {
    (void)s;
    (void)msg;
    (void)flags;
    return static_cast<s32>(len);
}

PS4_SYSV_ABI s32 sceNetRecv(s32 s, void* buf, usize len, s32 flags) {
    (void)s;
    (void)buf;
    (void)len;
    (void)flags;
    t_sce_net_errno = 35; // EAGAIN
    return -1;
}

PS4_SYSV_ABI s32 sceNetBind(s32 s, const void* name, s32 namelen) {
    (void)s;
    (void)name;
    (void)namelen;
    return 0;
}

PS4_SYSV_ABI s32 sceNetListen(s32 s, s32 backlog) {
    (void)s;
    (void)backlog;
    return 0;
}

PS4_SYSV_ABI s32 sceNetAccept(s32 s, void* addr, s32* addrlen) {
    (void)s;
    (void)addr;
    (void)addrlen;
    t_sce_net_errno = 35; // EAGAIN
    return -1;
}

PS4_SYSV_ABI s32 sceNetGetsockname(s32 s, void* name, s32* namelen) {
    (void)s;
    if (name && namelen && *namelen >= 16) {
        std::memset(name, 0, *namelen);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetGetpeername(s32 s, void* name, s32* namelen) {
    (void)s;
    if (name && namelen && *namelen >= 16) {
        std::memset(name, 0, *namelen);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceNetSetsockopt(s32 s, s32 level, s32 optname, const void* optval, s32 optlen) {
    (void)s;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return 0;
}

PS4_SYSV_ABI s32 sceNetResolverCreate(const char* name, void* mem, usize mem_size) {
    (void)name;
    (void)mem;
    (void)mem_size;
    static std::atomic<s32> s_next_resolver{1};
    return s_next_resolver.fetch_add(1);
}

PS4_SYSV_ABI s32 sceNetResolverAbort(s32 rid, u32 flags) {
    (void)rid;
    (void)flags;
    return 0;
}

PS4_SYSV_ABI s32 sceNetResolverGetError(s32 rid, s32* error) {
    (void)rid;
    if (error) {
        *error = 0;
    }
    return 0;
}

} // extern "C"

} // namespace Net
