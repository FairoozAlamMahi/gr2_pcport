#include "kernel/http_service.h"
#include "common/logger.h"
#include <cstring>
#include <string>
#include <atomic>
#include <algorithm>

namespace Http {

static std::atomic<s32> s_next_template_id{1};
static std::atomic<s32> s_next_connection_id{1};
static std::atomic<s32> s_next_request_id{1};

void Initialize() {
    LOG_INFO("HTTP", "HTTP/SSL/JSON services initialized");
}

void Shutdown() {
    LOG_INFO("HTTP", "HTTP/SSL/JSON services shutdown");
}

extern "C" {

// ============================================================================
// libSceHttp
// ============================================================================

PS4_SYSV_ABI s32 sceHttpInit(s32 mem_pool, s32 ssl_context_id, usize pool_size) {
    (void)mem_pool;
    (void)ssl_context_id;
    (void)pool_size;
    LOG_INFO("HTTP", "sceHttpInit: HTTP subsystem initialized (returning context ID 1)");
    return 1;
}

PS4_SYSV_ABI s32 sceHttpTerm(s32 http_ctx_id) {
    LOG_INFO("HTTP", "sceHttpTerm: HTTP subsystem terminated (ctx: " + std::to_string(http_ctx_id) + ")");
    return 0;
}

PS4_SYSV_ABI s32 sceHttpCreateTemplate(s32 http_ctx_id, const char* user_agent, s32 http_ver, s32 auto_redirect) {
    (void)http_ctx_id;
    (void)http_ver;
    (void)auto_redirect;
    s32 id = s_next_template_id.fetch_add(1, std::memory_order_relaxed);
    LOG_INFO("HTTP", "sceHttpCreateTemplate: created template ID " + std::to_string(id) + 
                     " (UA: " + (user_agent ? user_agent : "null") + ")");
    return id;
}

PS4_SYSV_ABI s32 sceHttpDeleteTemplate(s32 tmpl_id) {
    LOG_INFO("HTTP", "sceHttpDeleteTemplate: deleted template ID " + std::to_string(tmpl_id));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpCreateConnectionWithURL(s32 tmpl_id, const char* url, s32 is_keepalive) {
    (void)tmpl_id;
    (void)is_keepalive;
    s32 id = s_next_connection_id.fetch_add(1, std::memory_order_relaxed);
    LOG_INFO("HTTP", "sceHttpCreateConnectionWithURL: created conn ID " + std::to_string(id) + 
                     " (URL: " + (url ? url : "null") + ")");
    return id;
}

PS4_SYSV_ABI s32 sceHttpDeleteConnection(s32 conn_id) {
    LOG_INFO("HTTP", "sceHttpDeleteConnection: deleted conn ID " + std::to_string(conn_id));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpCreateRequestWithURL(s32 conn_id, s32 method, const char* url, u64 content_length) {
    (void)conn_id;
    (void)content_length;
    s32 id = s_next_request_id.fetch_add(1, std::memory_order_relaxed);
    LOG_INFO("HTTP", "sceHttpCreateRequestWithURL: created req ID " + std::to_string(id) + 
                     " (method: " + std::to_string(method) + ", URL: " + (url ? url : "null") + ")");
    return id;
}

PS4_SYSV_ABI s32 sceHttpDeleteRequest(s32 req_id) {
    LOG_INFO("HTTP", "sceHttpDeleteRequest: deleted req ID " + std::to_string(req_id));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSendRequest(s32 req_id, const void* post_data, usize size) {
    (void)post_data;
    LOG_INFO("HTTP", "sceHttpSendRequest: req ID " + std::to_string(req_id) + ", data size " + std::to_string(size));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpGetResponseContentLength(s32 req_id, u64* content_len) {
    if (content_len) {
        *content_len = 0;
    }
    LOG_INFO("HTTP", "sceHttpGetResponseContentLength: req ID " + std::to_string(req_id) + " -> len 0");
    return 0;
}

PS4_SYSV_ABI s32 sceHttpReadData(s32 req_id, void* data, u32 size) {
    (void)data;
    (void)size;
    LOG_INFO("HTTP", "sceHttpReadData: req ID " + std::to_string(req_id) + " -> EOF (0 bytes read)");
    return 0;
}

PS4_SYSV_ABI s32 sceHttpGetStatusCode(s32 req_id, s32* status_code) {
    if (status_code) {
        *status_code = 200; // 200 OK
    }
    LOG_INFO("HTTP", "sceHttpGetStatusCode: req ID " + std::to_string(req_id) + " -> 200 OK");
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetAutoRedirect(s32 id, s32 enable) {
    LOG_INFO("HTTP", "sceHttpSetAutoRedirect: ID " + std::to_string(id) + ", enable " + std::to_string(enable));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetAuthInfoCallback(s32 id, void* cb, void* user_arg) {
    (void)cb;
    (void)user_arg;
    LOG_INFO("HTTP", "sceHttpSetAuthInfoCallback: ID " + std::to_string(id));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetAuthEnabled(s32 id, s32 enable) {
    LOG_INFO("HTTP", "sceHttpSetAuthEnabled: ID " + std::to_string(id) + ", enable " + std::to_string(enable));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpAbortRequest(s32 req_id) {
    LOG_INFO("HTTP", "sceHttpAbortRequest: req ID " + std::to_string(req_id));
    return 0;
}

PS4_SYSV_ABI s32 sceHttpAddRequestHeader(s32 id, const char* name, const char* value, u32 mode) {
    (void)mode;
    LOG_INFO("HTTP", "sceHttpAddRequestHeader: ID " + std::to_string(id) + " [" + 
                     (name ? name : "") + ": " + (value ? value : "") + "]");
    return 0;
}

PS4_SYSV_ABI s32 sceHttpGetAllResponseHeaders(s32 req_id, char** headers, usize* headers_len) {
    (void)req_id;
    if (headers) {
        *headers = nullptr;
    }
    if (headers_len) {
        *headers_len = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceHttpGetLastErrno(s32 req_id, s32* err_num) {
    (void)req_id;
    if (err_num) {
        *err_num = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceHttpParseResponseHeader(s32 req_id, const char* header, void* out_val, usize* out_len) {
    (void)req_id;
    (void)header;
    (void)out_val;
    if (out_len) {
        *out_len = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetConnectTimeOut(s32 id, u32 usec) {
    (void)id;
    (void)usec;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetRecvTimeOut(s32 id, u32 usec) {
    (void)id;
    (void)usec;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetSendTimeOut(s32 id, u32 usec) {
    (void)id;
    (void)usec;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetResolveRetry(s32 id, s32 retry) {
    (void)id;
    (void)retry;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpSetResolveTimeOut(s32 id, u32 usec) {
    (void)id;
    (void)usec;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpsGetSslError(s32 req_id, u32* ssl_error) {
    (void)req_id;
    if (ssl_error) {
        *ssl_error = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceHttpsSetSslCallback(s32 id, void* cb, void* user_arg) {
    (void)id;
    (void)cb;
    (void)user_arg;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpsDisableOption(s32 id, u32 opt) {
    (void)id;
    (void)opt;
    return 0;
}

PS4_SYSV_ABI s32 sceHttpUriEscape(char* out, usize* out_len, const char* in, usize in_len) {
    if (!in || !out_len) {
        return -1;
    }
    std::string escaped;
    escaped.reserve(in_len * 3);
    for (usize i = 0; i < in_len; ++i) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped.push_back(static_cast<char>(c));
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            escaped.append(hex);
        }
    }
    if (!out) {
        *out_len = escaped.size();
        return 0;
    }
    usize copy_len = (std::min)(*out_len, escaped.size());
    if (copy_len > 0) {
        std::memcpy(out, escaped.data(), copy_len);
    }
    if (copy_len < *out_len) {
        out[copy_len] = '\0';
    }
    *out_len = escaped.size();
    return 0;
}

// ============================================================================
// libSceSsl
// ============================================================================

PS4_SYSV_ABI s32 sceSslInit(usize pool_size) {
    LOG_INFO("SSL", "sceSslInit: SSL subsystem initialized (pool: " + std::to_string(pool_size) + " bytes)");
    return 1;
}

PS4_SYSV_ABI s32 sceSslTerm(s32 ssl_id) {
    LOG_INFO("SSL", "sceSslTerm: SSL subsystem terminated (id: " + std::to_string(ssl_id) + ")");
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetSerialNumber(void* cert, void* serial, usize* serial_len) {
    (void)cert;
    (void)serial;
    if (serial_len) {
        *serial_len = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetMemoryPoolStats(void* stats) {
    if (stats) {
        std::memset(stats, 0, 32);
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSslFreeSslCertName(void* name) {
    (void)name;
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetIssuerName(void* cert, void** name) {
    (void)cert;
    if (name) {
        *name = nullptr;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetNameEntryCount(void* name, s32* count) {
    (void)name;
    if (count) {
        *count = 0;
    }
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetNameEntryInfo(void* name, s32 index, void* info) {
    (void)name;
    (void)index;
    (void)info;
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetNotAfter(void* cert, void* time) {
    (void)cert;
    (void)time;
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetNotBefore(void* cert, void* time) {
    (void)cert;
    (void)time;
    return 0;
}

PS4_SYSV_ABI s32 sceSslGetSubjectName(void* cert, void** name) {
    (void)cert;
    if (name) {
        *name = nullptr;
    }
    return 0;
}

// ============================================================================
// libSceJson / libSceJson2
// ============================================================================

PS4_SYSV_ABI s32 sceJsonInit(void* mem, usize mem_size) {
    (void)mem;
    (void)mem_size;
    LOG_INFO("JSON", "sceJsonInit: JSON subsystem initialized");
    return 0;
}

PS4_SYSV_ABI s32 sceJsonTerm() {
    LOG_INFO("JSON", "sceJsonTerm: JSON subsystem terminated");
    return 0;
}

// Initializer C++ thunks
PS4_SYSV_ABI s32 _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* this_ptr, const void* param) {
    (void)this_ptr;
    (void)param;
    LOG_INFO("JSON", "sce::Json::Initializer::initialize called");
    return 0;
}

PS4_SYSV_ABI s32 _ZN3sce4Json11Initializer9terminateEv(void* this_ptr) {
    (void)this_ptr;
    LOG_INFO("JSON", "sce::Json::Initializer::terminate called");
    return 0;
}

PS4_SYSV_ABI void* _ZN3sce4Json11InitializerC1Ev(void* this_ptr) {
    LOG_INFO("JSON", "sce::Json::Initializer::Initializer called");
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json11InitializerD1Ev(void* this_ptr) {
    (void)this_ptr;
    LOG_INFO("JSON", "sce::Json::Initializer::~Initializer called");
}

// Value C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json5Value3setERKS1_(void* this_ptr, const void* other) {
    (void)other;
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json5Value5clearEv(void* this_ptr) {
    (void)this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Ed(void* this_ptr, double d) {
    (void)d;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1El(void* this_ptr, s64 l) {
    (void)l;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Em(void* this_ptr, u64 m) {
    (void)m;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Ev(void* this_ptr) {
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json5ValueD1Ev(void* this_ptr) {
    (void)this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueaSERKS1_(void* this_ptr, const void* other) {
    (void)other;
    return this_ptr;
}

PS4_SYSV_ABI s32 _ZN3sce4Json5Value9serializeERNS0_6StringE(void* this_ptr, void* str) {
    (void)this_ptr;
    (void)str;
    return 0;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(void* this_ptr, const void* arr) {
    (void)arr;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(void* this_ptr, const void* obj) {
    (void)obj;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_6StringE(void* this_ptr, const void* str) {
    (void)str;
    return this_ptr;
}

// Object C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json6ObjectC1ERKS1_(void* this_ptr, const void* other) {
    (void)other;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json6ObjectC1Ev(void* this_ptr) {
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json6ObjectD1Ev(void* this_ptr) {
    (void)this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json6ObjectaSERKS1_(void* this_ptr, const void* other) {
    (void)other;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json6ObjectixERKNS0_6StringE(void* this_ptr, const void* str) {
    (void)str;
    static u8 s_dummy_val[64]{};
    return s_dummy_val;
}

// String C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json6StringC1EPKc(void* this_ptr, const char* s) {
    (void)s;
    return this_ptr;
}

PS4_SYSV_ABI void* _ZN3sce4Json6StringC1Ev(void* this_ptr) {
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json6StringD1Ev(void* this_ptr) {
    (void)this_ptr;
}

PS4_SYSV_ABI const char* _ZNK3sce4Json6String5c_strEv(const void* this_ptr) {
    (void)this_ptr;
    static const char empty[] = "";
    return empty;
}

// Array C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json5ArrayC1Ev(void* this_ptr) {
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json5ArrayD1Ev(void* this_ptr) {
    (void)this_ptr;
}

PS4_SYSV_ABI s32 _ZN3sce4Json5Array9push_backERKNS0_5ValueE(void* this_ptr, const void* val) {
    (void)this_ptr;
    (void)val;
    return 0;
}

PS4_SYSV_ABI usize _ZNK3sce4Json5Array4sizeEv(const void* this_ptr) {
    (void)this_ptr;
    return 0;
}

// MemAllocator C++ thunks
PS4_SYSV_ABI s32 _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void* this_ptr, s32 err, usize size, void* ptr) {
    (void)this_ptr;
    (void)err;
    (void)size;
    (void)ptr;
    return 0;
}

PS4_SYSV_ABI void* _ZN3sce4Json12MemAllocatorC2Ev(void* this_ptr) {
    return this_ptr;
}

PS4_SYSV_ABI void _ZN3sce4Json12MemAllocatorD2Ev(void* this_ptr) {
    (void)this_ptr;
}

} // extern "C"

} // namespace Http
