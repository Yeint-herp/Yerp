#ifndef SUPERVISOR_ARCH_X86_64_IOAPIC_H
#define SUPERVISOR_ARCH_X86_64_IOAPIC_H

#include <arch/Interrupts.h>
#include <arch/x86_64/Apic.h>
#include <core/Spinlock.h>

#define IOAPIC_RTE_MASKED     (1ULL << 16)
#define IOAPIC_RTE_LEVEL      (1ULL << 15)
#define IOAPIC_RTE_ACTIVE_LOW (1ULL << 13)
#define IOAPIC_RTE_LOGICAL    (1ULL << 11)

#define IOAPIC_RTE_DELIV_FIXED  (0ULL << 8)
#define IOAPIC_RTE_DELIV_LOWEST (1ULL << 8)
#define IOAPIC_RTE_DELIV_SMI    (2ULL << 8)
#define IOAPIC_RTE_DELIV_NMI    (4ULL << 8)
#define IOAPIC_RTE_DELIV_INIT   (5ULL << 8)
#define IOAPIC_RTE_DELIV_EXTINT (7ULL << 8)

#define IOAPIC_RTE_DEST_SHIFT 56
#define IOAPIC_MAX_GSI        256

typedef struct
{
    u8   Vector;
    u32  OwnerApicId;
    u32  OwnerProcessorNumber;
    bool Active;
} X86_64_IoApicGsiBinding;

void X86_64_IoApicInit(void);

bool X86_64_IoApicRouteGsi(u32 gsi, u8 vector, Interrupt_Flags flags, u32 destApicId, u32 destProcessorNumber);

void X86_64_IoApicMaskGsi(u32 gsi);
void X86_64_IoApicUnmaskGsi(u32 gsi);

u64 X86_64_IoApicReadRte(u32 gsi);

const X86_64_IoApicGsiBinding *X86_64_IoApicGetBinding(u32 gsi);

#endif /* SUPERVISOR_ARCH_X86_64_IOAPIC_H */
