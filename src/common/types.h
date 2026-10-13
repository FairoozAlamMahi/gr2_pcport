#pragma once

#include <cstdint>
#include <cstddef>

// Ensure Clang with System V AMD64 ABI is used
#if defined(_MSC_VER) && !defined(__clang__)
#error "LLVM/Clang is required to compile gr2port with __attribute__((sysv_abi))"
#endif

// PlayStation 4 Orbis OS Calling Convention Macro
#define PS4_SYSV_ABI __attribute__((sysv_abi))

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using usize = size_t;
using isize = ptrdiff_t;

// Orbis Base Address Arena (1 TiB virtual arena)
constexpr u64 GUEST_ARENA_BASE = 0x10000000000ULL;
constexpr u64 GUEST_ARENA_SIZE = 0x10000000000ULL; // 1 TiB address space
