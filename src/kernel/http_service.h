#pragma once

#include "common/types.h"

namespace Http {

void Initialize();
void Shutdown();

extern "C" {

// ============================================================================
// libSceHttp
// ============================================================================
PS4_SYSV_ABI s32 sceHttpInit(s32 mem_pool, s32 ssl_context_id, usize pool_size);
PS4_SYSV_ABI s32 sceHttpTerm(s32 http_ctx_id);
PS4_SYSV_ABI s32 sceHttpCreateTemplate(s32 http_ctx_id, const char* user_agent, s32 http_ver, s32 auto_redirect);
PS4_SYSV_ABI s32 sceHttpDeleteTemplate(s32 tmpl_id);
PS4_SYSV_ABI s32 sceHttpCreateConnectionWithURL(s32 tmpl_id, const char* url, s32 is_keepalive);
PS4_SYSV_ABI s32 sceHttpDeleteConnection(s32 conn_id);
PS4_SYSV_ABI s32 sceHttpCreateRequestWithURL(s32 conn_id, s32 method, const char* url, u64 content_length);
PS4_SYSV_ABI s32 sceHttpDeleteRequest(s32 req_id);
PS4_SYSV_ABI s32 sceHttpSendRequest(s32 req_id, const void* post_data, usize size);
PS4_SYSV_ABI s32 sceHttpGetResponseContentLength(s32 req_id, u64* content_len);
PS4_SYSV_ABI s32 sceHttpReadData(s32 req_id, void* data, u32 size);
PS4_SYSV_ABI s32 sceHttpGetStatusCode(s32 req_id, s32* status_code);
PS4_SYSV_ABI s32 sceHttpSetAutoRedirect(s32 id, s32 enable);
PS4_SYSV_ABI s32 sceHttpSetAuthInfoCallback(s32 id, void* cb, void* user_arg);
PS4_SYSV_ABI s32 sceHttpSetAuthEnabled(s32 id, s32 enable);
PS4_SYSV_ABI s32 sceHttpAbortRequest(s32 req_id);
PS4_SYSV_ABI s32 sceHttpAddRequestHeader(s32 id, const char* name, const char* value, u32 mode);
PS4_SYSV_ABI s32 sceHttpGetAllResponseHeaders(s32 req_id, char** headers, usize* headers_len);
PS4_SYSV_ABI s32 sceHttpGetLastErrno(s32 req_id, s32* err_num);
PS4_SYSV_ABI s32 sceHttpParseResponseHeader(s32 req_id, const char* header, void* out_val, usize* out_len);
PS4_SYSV_ABI s32 sceHttpSetConnectTimeOut(s32 id, u32 usec);
PS4_SYSV_ABI s32 sceHttpSetRecvTimeOut(s32 id, u32 usec);
PS4_SYSV_ABI s32 sceHttpSetSendTimeOut(s32 id, u32 usec);
PS4_SYSV_ABI s32 sceHttpSetResolveRetry(s32 id, s32 retry);
PS4_SYSV_ABI s32 sceHttpSetResolveTimeOut(s32 id, u32 usec);
PS4_SYSV_ABI s32 sceHttpsGetSslError(s32 req_id, u32* ssl_error);
PS4_SYSV_ABI s32 sceHttpsSetSslCallback(s32 id, void* cb, void* user_arg);
PS4_SYSV_ABI s32 sceHttpsDisableOption(s32 id, u32 opt);
PS4_SYSV_ABI s32 sceHttpUriEscape(char* out, usize* out_len, const char* in, usize in_len);

// ============================================================================
// libSceSsl
// ============================================================================
PS4_SYSV_ABI s32 sceSslInit(usize pool_size);
PS4_SYSV_ABI s32 sceSslTerm(s32 ssl_id);
PS4_SYSV_ABI s32 sceSslGetSerialNumber(void* cert, void* serial, usize* serial_len);
PS4_SYSV_ABI s32 sceSslGetMemoryPoolStats(void* stats);
PS4_SYSV_ABI s32 sceSslFreeSslCertName(void* name);
PS4_SYSV_ABI s32 sceSslGetIssuerName(void* cert, void** name);
PS4_SYSV_ABI s32 sceSslGetNameEntryCount(void* name, s32* count);
PS4_SYSV_ABI s32 sceSslGetNameEntryInfo(void* name, s32 index, void* info);
PS4_SYSV_ABI s32 sceSslGetNotAfter(void* cert, void* time);
PS4_SYSV_ABI s32 sceSslGetNotBefore(void* cert, void* time);
PS4_SYSV_ABI s32 sceSslGetSubjectName(void* cert, void** name);

// ============================================================================
// libSceJson / libSceJson2
// ============================================================================
PS4_SYSV_ABI s32 sceJsonInit(void* mem, usize mem_size);
PS4_SYSV_ABI s32 sceJsonTerm();

// Initializer C++ thunks
PS4_SYSV_ABI s32 _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* this_ptr, const void* param);
PS4_SYSV_ABI s32 _ZN3sce4Json11Initializer9terminateEv(void* this_ptr);
PS4_SYSV_ABI void* _ZN3sce4Json11InitializerC1Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json11InitializerD1Ev(void* this_ptr);

// Value C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json5Value3setERKS1_(void* this_ptr, const void* other);
PS4_SYSV_ABI void _ZN3sce4Json5Value5clearEv(void* this_ptr);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Ed(void* this_ptr, double d);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1El(void* this_ptr, s64 l);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Em(void* this_ptr, u64 m);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json5ValueD1Ev(void* this_ptr);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueaSERKS1_(void* this_ptr, const void* other);
PS4_SYSV_ABI s32 _ZN3sce4Json5Value9serializeERNS0_6StringE(void* this_ptr, void* str);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(void* this_ptr, const void* arr);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(void* this_ptr, const void* obj);
PS4_SYSV_ABI void* _ZN3sce4Json5ValueC1ERKNS0_6StringE(void* this_ptr, const void* str);

// Object C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json6ObjectC1ERKS1_(void* this_ptr, const void* other);
PS4_SYSV_ABI void* _ZN3sce4Json6ObjectC1Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json6ObjectD1Ev(void* this_ptr);
PS4_SYSV_ABI void* _ZN3sce4Json6ObjectaSERKS1_(void* this_ptr, const void* other);
PS4_SYSV_ABI void* _ZN3sce4Json6ObjectixERKNS0_6StringE(void* this_ptr, const void* str);

// String C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json6StringC1EPKc(void* this_ptr, const char* s);
PS4_SYSV_ABI void* _ZN3sce4Json6StringC1Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json6StringD1Ev(void* this_ptr);
PS4_SYSV_ABI const char* _ZNK3sce4Json6String5c_strEv(const void* this_ptr);

// Array C++ thunks
PS4_SYSV_ABI void* _ZN3sce4Json5ArrayC1Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json5ArrayD1Ev(void* this_ptr);
PS4_SYSV_ABI s32 _ZN3sce4Json5Array9push_backERKNS0_5ValueE(void* this_ptr, const void* val);
PS4_SYSV_ABI usize _ZNK3sce4Json5Array4sizeEv(const void* this_ptr);

// MemAllocator C++ thunks
PS4_SYSV_ABI s32 _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void* this_ptr, s32 err, usize size, void* ptr);
PS4_SYSV_ABI void* _ZN3sce4Json12MemAllocatorC2Ev(void* this_ptr);
PS4_SYSV_ABI void _ZN3sce4Json12MemAllocatorD2Ev(void* this_ptr);

} // extern "C"

} // namespace Http
