#ifndef SUPERVISOR_CONST_H
#define SUPERVISOR_CONST_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

typedef signed char	 i8;
typedef signed short i16;
typedef signed int	 i32;
typedef signed long	 i64;

typedef u64 usize;
typedef i64 isize;

typedef usize uptr;
typedef isize iptr;

typedef uptr paddr_t;
typedef uptr vaddr_t;

#define unreachable() __builtin_unreachable()

#endif /* SUPERVISOR_CONST_H */
