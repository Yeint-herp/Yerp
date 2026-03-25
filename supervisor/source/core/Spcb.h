#ifndef SUPERVISOR_CORE_SPCB_H
#define SUPERVISOR_CORE_SPCB_H

#include <arch/Atomic.h>
#include <arch/Spcr.h>
#include <mm/Magazine.h>

struct Core_SPCB
{
    struct Core_SPCB *Self;

    u32 ProcessorNumber;
    u8  CurrentIrql;
    u8  Reserved[3];

    u64 ScratchZone;

    struct Mm_PfaMagazine FreePages;
    struct Mm_PfaMagazine ZeroPages;

    struct Arch_SPCR ArchData;
    ArchId_t         ArchId;

    Arch_Atomic32 SlabTid;
};

struct limine_mp_response;

/// allocates the SPCB array for all detected cores during early boot.
bool Core_SpcbAllocateAll(struct limine_mp_response *mpResponse);

/// called by each core to init it's SPCB.
bool Core_SpcbInit(struct Core_SPCB *spcb);
bool Core_SpcbLateInit(struct Core_SPCB *spcb);

u32 Core_GetProcessorCount(void);

#endif /* SUPERVISOR_CORE_SPCB_H */
