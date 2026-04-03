#ifndef SUPERVISOR_ARCH_X86_64_TSC_H
#define SUPERVISOR_ARCH_X86_64_TSC_H

#include <core/Memory.h>

typedef struct
{
    u64  FreqHz;
    u32  FreqKhz;
    bool IsInvariant;
    bool IsAvailable;
} X86_64_TscInfo;

void                  X86_64_TscInit(void);
const X86_64_TscInfo *X86_64_TscGetInfo(void);

u64 X86_64_TscTicksToNs(u64 ticks);
u64 X86_64_TscNsToTicks(u64 ns);

u64 X86_64_TscTicksToUs(const X86_64_TscInfo *info, u64 ticks);
u64 X86_64_TscUsToTicks(const X86_64_TscInfo *info, u64 us);

u64 X86_64_TscRead(void);
u64 X86_64_TscReadSerializing(u32 *auxOut);

#endif /* SUPERVISOR_ARCH_X86_64_TSC_H */
