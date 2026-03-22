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

#define unreachable()  __builtin_unreachable()
#define ArraySize(arr) sizeof(arr) / sizeof(*arr)

#define Bit_FirstSetIndex(x)   __builtin_ctz(x)
#define Bit_FirstClearIndex(x) Bit_FirstSetIndex(~x)

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#define container_of(ptr, type, member)                                                                                \
    ({                                                                                                                 \
        const typeof(((type *)0)->member) *__mptr = (ptr);                                                             \
        (type *)((char *)__mptr - offsetof(type, member));                                                             \
    })

#endif /* SUPERVISOR_CONST_H */
