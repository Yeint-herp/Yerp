#define DBG_MODULE "IoApic"

#include <arch/Io.h>
#include <arch/MmArch.h>
#include <arch/x86_64/IoApic.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>

#define IOAPIC_REGSEL 0x00
#define IOAPIC_REGWIN 0x10

#define IOAPIC_REG_ID     0x00
#define IOAPIC_REG_VER    0x01
#define IOAPIC_REG_REDTBL 0x10

static X86_64_IoApicGsiBinding s_GsiBindings[IOAPIC_MAX_GSI];
static Core_Spinlock           s_GsiLock;

static inline void s_WriteReg(uptr base, u8 reg, u32 value)
{
    Arch_MmioWrite32((void *)(base + IOAPIC_REGSEL), reg);
    Arch_MmioWrite32((void *)(base + IOAPIC_REGWIN), value);
}

static inline u32 s_ReadReg(uptr base, u8 reg)
{
    Arch_MmioWrite32((void *)(base + IOAPIC_REGSEL), reg);
    return Arch_MmioRead32((void *)(base + IOAPIC_REGWIN));
}

static void s_WriteRte(const X86_64_ApicIoApicInfo *io, u32 pin, u64 rte)
{
    const u8 regLo = IOAPIC_REG_REDTBL + pin * 2;
    const u8 regHi = regLo + 1;

    s_WriteReg(io->MmioBase, regHi, rte >> 32);
    s_WriteReg(io->MmioBase, regLo, rte & 0xFFFFFFFF);
}

static u64 s_ReadRte(const X86_64_ApicIoApicInfo *io, u32 pin)
{
    const u8 regLo = IOAPIC_REG_REDTBL + pin * 2;
    const u8 regHi = regLo + 1;

    const u32 lo = s_ReadReg(io->MmioBase, regLo);
    const u32 hi = s_ReadReg(io->MmioBase, regHi);

    return ((u64)hi << 32) | lo;
}

static bool s_ResolveGsi(u32 gsi, const X86_64_ApicIoApicInfo **outIo, u32 *outPin)
{
    const X86_64_ApicIoApicInfo *io = X86_64_ApicFindIoApicForGsi(gsi);

    if (!io)
    {
        Log(ERROR, "no IOAPIC owns GSI %u", gsi);
        return false;
    }

    *outIo  = io;
    *outPin = gsi - io->GsiBase;
    return true;
}

static u64 s_BuildRteFlags(Interrupt_Flags flags)
{
    u64 rte = 0;

    switch (flags.polarity)
    {
        case kInterruptPolarityActiveLow:
            rte |= IOAPIC_RTE_ACTIVE_LOW;
            break;
        case kInterruptPolarityActiveHigh:
        case kInterruptPolarityDefault:
            break;
    }

    switch (flags.trigger)
    {
        case kInterruptTriggerLevel:
            rte |= IOAPIC_RTE_LEVEL;
            break;
        case kInterruptTriggerEdge:
        case kInterruptTriggerDefault:
            break;
    }

    return rte;
}

void X86_64_IoApicInit(void)
{
    const X86_64_ApicState *state = X86_64_ApicGetState();

    Dsa_VectorForEach(state->IoApics, io)
    {
        for (u32 pin = 0; pin < io->GsiCount; pin++)
        {
            u64 rte = IOAPIC_RTE_MASKED | IOAPIC_RTE_DELIV_FIXED;
            s_WriteRte(io, pin, rte);
        }

        Log(INFO, "IOAPIC %u: masked %u pins (GSI %u..%u)", io->IoApicId, io->GsiCount, io->GsiBase,
            io->GsiBase + io->GsiCount - 1);
    }
}

bool X86_64_IoApicRouteGsi(u32 gsi, u8 vector, Interrupt_Flags flags, u32 destApicId, u32 destProcessorNumber)
{
    if (gsi >= IOAPIC_MAX_GSI)
    {
        Log(ERROR, "GSI %u exceeds max %u", gsi, IOAPIC_MAX_GSI);
        return false;
    }

    const X86_64_ApicIoApicInfo *io;
    u32                          pin;

    if (!s_ResolveGsi(gsi, &io, &pin))
        return false;

    u64 rte = IOAPIC_RTE_MASKED | IOAPIC_RTE_DELIV_FIXED | s_BuildRteFlags(flags) |
              ((u64)destApicId << IOAPIC_RTE_DEST_SHIFT) | vector;

    Core_SpinlockAcquire(&s_GsiLock);

    s_WriteRte(io, pin, rte);
    s_GsiBindings[gsi] = (X86_64_IoApicGsiBinding){
        .Vector               = vector,
        .OwnerApicId          = destApicId,
        .OwnerProcessorNumber = destProcessorNumber,
        .Active               = true,
    };

    Core_SpinlockRelease(&s_GsiLock);

    Log(TRACE, "GSI %u -> vector %u, dest APIC %u (core %u), pin %u on IOAPIC %u", gsi, vector, destApicId,
        destProcessorNumber, pin, io->IoApicId);

    return true;
}

void X86_64_IoApicMaskGsi(u32 gsi)
{
    const X86_64_ApicIoApicInfo *io;
    u32                          pin;

    if (!s_ResolveGsi(gsi, &io, &pin))
        return;

    u64 rte = s_ReadRte(io, pin);
    rte |= IOAPIC_RTE_MASKED;
    s_WriteRte(io, pin, rte);
}

void X86_64_IoApicUnmaskGsi(u32 gsi)
{
    const X86_64_ApicIoApicInfo *io;
    u32                          pin;

    if (!s_ResolveGsi(gsi, &io, &pin))
        return;

    u64 rte = s_ReadRte(io, pin);
    rte &= ~IOAPIC_RTE_MASKED;
    s_WriteRte(io, pin, rte);
}

u64 X86_64_IoApicReadRte(u32 gsi)
{
    const X86_64_ApicIoApicInfo *io;
    u32                          pin;

    if (!s_ResolveGsi(gsi, &io, &pin))
        return IOAPIC_RTE_MASKED;

    return s_ReadRte(io, pin);
}
