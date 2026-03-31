#ifndef SUPERVISOR_ARCH_X86_64_APIC_H
#define SUPERVISOR_ARCH_X86_64_APIC_H

#include <core/Memory.h>
#include <dsa/Vector.h>

typedef struct
{
    u8   IoApicId;
    u32  GsiBase;
    u32  GsiCount;
    u64  PhysBase;
    uptr MmioBase;
} X86_64_ApicIoApicInfo;

typedef struct
{
    u8  Bus;
    u8  Source;
    u32 Gsi;
    u16 Flags;
} X86_64_ApicIsoInfo;

typedef struct
{
    u8  AcpiProcessorId;
    u32 ApicId;
    u32 Flags;
} X86_64_ApicLapicInfo;

typedef struct
{
    u8  Lint;
    u16 Flags;
} X86_64_ApicNmiInfo;

typedef struct
{
    u64  LapicBase;
    bool x2apic;

    vector_of(X86_64_ApicLapicInfo) Lapics;
    vector_of(X86_64_ApicIoApicInfo) IoApics;
    vector_of(X86_64_ApicIsoInfo) Isos;
    vector_of(X86_64_ApicNmiInfo) Nmis;
} X86_64_ApicState;

void X86_64_ApicDiscover(void);

const X86_64_ApicState *X86_64_ApicGetState(void);

u32                          X86_64_ApicIsaIrqToGsi(u8 isaIrq, u16 *outFlags);
const X86_64_ApicIoApicInfo *X86_64_ApicFindIoApicForGsi(u32 gsi);

#endif /* SUPERVISOR_ARCH_X86_64_APIC_H */
