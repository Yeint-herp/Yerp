#ifndef SUPERVISOR_ARCH_X86_64_CORE_LOCAL_H
#define SUPERVISOR_ARCH_X86_64_CORE_LOCAL_H

#define Arch_ReadCoreLocal8(offset)                                                                                    \
    ({                                                                                                                 \
        u8 __val;                                                                                                      \
        __asm__ volatile("movb %%gs:%c1, %0" : "=r"(__val) : "i"(offset));                                             \
        __val;                                                                                                         \
    })

#define Arch_ReadCoreLocal16(offset)                                                                                   \
    ({                                                                                                                 \
        u16 __val;                                                                                                     \
        __asm__ volatile("movw %%gs:%c1, %0" : "=r"(__val) : "i"(offset));                                             \
        __val;                                                                                                         \
    })

#define Arch_ReadCoreLocal32(offset)                                                                                   \
    ({                                                                                                                 \
        u32 __val;                                                                                                     \
        __asm__ volatile("movl %%gs:%c1, %0" : "=r"(__val) : "i"(offset));                                             \
        __val;                                                                                                         \
    })

#define Arch_ReadCoreLocal64(offset)                                                                                   \
    ({                                                                                                                 \
        u64 __val;                                                                                                     \
        __asm__ volatile("movq %%gs:%c1, %0" : "=r"(__val) : "i"(offset));                                             \
        __val;                                                                                                         \
    })

#define Arch_WriteCoreLocal8(offset, val)  ({ __asm__ volatile("movb %0, %%gs:%c1" : : "r"((u8)(val)), "i"(offset)); })
#define Arch_WriteCoreLocal16(offset, val) ({ __asm__ volatile("movw %0, %%gs:%c1" : : "r"((u16)(val)), "i"(offset)); })
#define Arch_WriteCoreLocal32(offset, val) ({ __asm__ volatile("movl %0, %%gs:%c1" : : "r"((u32)(val)), "i"(offset)); })
#define Arch_WriteCoreLocal64(offset, val) ({ __asm__ volatile("movq %0, %%gs:%c1" : : "r"((u64)(val)), "i"(offset)); })

#endif /* SUPERVISOR_ARCH_X86_64_CORE_LOCAL_H */
