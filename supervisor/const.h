#ifndef SUPERVISOR_CONST_H
#define SUPERVISOR_CONST_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      i8;
typedef signed short     i16;
typedef signed int       i32;
typedef signed long long i64;

typedef typeof(sizeof(u8)) usize;
typedef signed long        isize;

typedef usize uptr;
typedef isize iptr;

typedef uptr paddr_t;
typedef uptr vaddr_t;

#define U64_MAX ((u64) - 1)
#define U32_MAX ((u32) - 1)

#define unreachable()  __builtin_unreachable()
#define counted_by(x) __attribute__((counted_by(x)))
#define ArraySize(arr) sizeof(arr) / sizeof(*arr)

#define Bit_FirstSetIndex(x)   __builtin_ctz(x)
#define Bit_FirstClearIndex(x) Bit_FirstSetIndex(~x)

#define Bitmap_TestBit(bitmap, vec)  (Arch_AtomicLoad32(&(bitmap)[(vec) / 32]) & (1u << ((vec) % 32)))
#define Bitmap_SetBit(bitmap, vec)   Arch_AtomicAdd32(&(bitmap)[(vec) / 32], 1u << ((vec) % 32))
#define Bitmap_ClearBit(bitmap, vec) Arch_AtomicSub32(&(bitmap)[(vec) / 32], 1u << ((vec) % 32))

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#define AlignDown(val, align) ((val) & ~((typeof(val))(align) - 1))
#define AlignUp(val, align)   (((val) + ((typeof(val))(align) - 1)) & ~((typeof(val))(align) - 1))

#define IsAligned(val, align) (((val) & ((typeof(val))(align) - 1)) == 0)

#define container_of(ptr, type, member)                                                                                \
    ({                                                                                                                 \
        const typeof(((type *)0)->member) *__mptr = (ptr);                                                             \
        (type *)((char *)__mptr - offsetof(type, member));                                                             \
    })

#define _MacroArgCount(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define MacroArgCount(...)                                     _MacroArgCount(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)

#define _MacroConcat(a, b)       a##b
#define MacroConcat(a, b)        _MacroConcat(a, b)
#define MacroDispatch(base, ...) MacroConcat(base, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)

#endif /* SUPERVISOR_CONST_H */
