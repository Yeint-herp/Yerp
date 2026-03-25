#ifndef SUPERVISOR_ARCH_CORE_LOCAL_H
#define SUPERVISOR_ARCH_CORE_LOCAL_H

#include <executive/Init.h>

struct Core_SPCB;

void              Arch_SetCoreSpcb(struct Core_SPCB *spcb);
struct Core_SPCB *Arch_GetCurrentSpcb(void);

#if kArch == x86_64
#include <arch/x86_64/CoreLocal.h>
#else
#error "Invalid architecture"
#endif

#define Arch_ReadLocal(field)                                                                                          \
    _Generic(((struct Core_SPCB *)0)->field,                                                                           \
        u8: Arch_ReadCoreLocal8(offsetof(struct Core_SPCB, field)),                                                    \
        u16: Arch_ReadCoreLocal16(offsetof(struct Core_SPCB, field)),                                                  \
        u32: Arch_ReadCoreLocal32(offsetof(struct Core_SPCB, field)),                                                  \
        u64: Arch_ReadCoreLocal64(offsetof(struct Core_SPCB, field)))

#define Arch_WriteLocal(field, val)                                                                                    \
    _Generic(((struct Core_SPCB *)0)->field,                                                                           \
        u8: Arch_WriteCoreLocal8(offsetof(struct Core_SPCB, field), val),                                              \
        u16: Arch_WriteCoreLocal16(offsetof(struct Core_SPCB, field), val),                                            \
        u32: Arch_WriteCoreLocal32(offsetof(struct Core_SPCB, field), val),                                            \
        u64: Arch_WriteCoreLocal64(offsetof(struct Core_SPCB, field), val))

#endif /* SUPERVISOR_ARCH_CORE_LOCAL_H */
