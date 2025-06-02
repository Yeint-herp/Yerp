/* this file is included into all TUs */

#ifndef TYPE_H
#define TYPE_H

#define TARGET_X86_64 1

#ifdef C4_X86_64
    #define TARGET TARGET_X86_64
#endif

#ifdef __cplusplus

static constexpr bool kDebugMode = true;
static constexpr bool kPanicOnError = false;
static constexpr bool kIdtUseTrapGateForExceptions = true;
static constexpr bool kIdtPanicOnException = true;
static constexpr bool kPmmZeroOnAlloc = false;
static constexpr bool kPmmZeroOnFree = false;
static constexpr bool kPmmUseFifo = false;

#include <stdint.h>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

static_assert(sizeof(u8) == 1, "u8 must be 8 bits");
static_assert(sizeof(u16) == 2, "u16 must be 16 bits");
static_assert(sizeof(u32) == 4, "u32 must be 32 bits");
static_assert(sizeof(u64) == 8, "u64 must be 64 bits");

static_assert(sizeof(i8) == 1, "i8 must be 8 bits");
static_assert(sizeof(i16) == 2, "i16 must be 16 bits");
static_assert(sizeof(i32) == 4, "i32 must be 32 bits");
static_assert(sizeof(i64) == 8, "i64 must be 64 bits");

using usize = decltype(sizeof(u8));
using isize = decltype(sizeof(u8));

using uptr = u64;

static_assert(sizeof(uptr) == sizeof(void*), "uptr must be native platform bit size");

inline void* operator new(usize, void* ptr) noexcept { return ptr; }
inline void operator delete(void*, void*) noexcept { }

#endif

#endif // TYPE_H
