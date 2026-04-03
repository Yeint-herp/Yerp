#ifndef SUPERVISOR_CORE_SPCB_H
#define SUPERVISOR_CORE_SPCB_H

#include <arch/Atomic.h>
#include <arch/Spcr.h>
#include <executive/Timer.h>
#include <core/Spinlock.h>
#include <dsa/RingBuffer.h>
#include <executive/Dpc.h>
#include <mm/Magazine.h>

#define DPC_QUEUE_DEPTH 256

struct Core_SPCB
{
    struct Core_SPCB *Self;

    u32 ProcessorNumber;
    u8  CurrentIrql;
    u8  Reserved[3];

    u64 ScratchZone;

    struct Mm_PfaMagazine FreePages;
    struct Mm_PfaMagazine ZeroPages;

    Core_Spinlock DpcQueueLock;
    ringbuf_of(Dpc*, DPC_QUEUE_DEPTH) DpcQueue;

    Ex_TimerCpu Timers;

    struct Arch_SPCR ArchData;
    ArchId_t         ArchId;

    Arch_Atomic32 SlabTid;
};

struct limine_mp_response;

/// allocates the SPCB array for all detected cores during early boot.
bool Core_SpcbAllocateAll(struct limine_mp_response *mpResponse);

/// boots all APs, waits for them to init and check in, then returns.
void Core_SpcbBootAll(struct limine_mp_response *mpResponse);

/// releases parked APs into the dispatcher.
void Core_SpcbReleaseAps(void);

/// called by each core to init it's SPCB.
bool Core_SpcbInit(struct Core_SPCB *spcb);
bool Core_SpcbLateInit(struct Core_SPCB *spcb);

u32               Core_GetProcessorCount(void);
struct Core_SPCB *Core_SpcbGetByNumber(u32 processorNumber);

#endif /* SUPERVISOR_CORE_SPCB_H */
