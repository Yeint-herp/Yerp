#define DBG_MODULE "Interrupt"

#include <arch/CoreLocal.h>
#include <arch/CpuHint.h>
#include <arch/Interrupts.h>
#include <arch/x86_64/Spcr.h>
#include <core/Spcb.h>
#include <core/Spinlock.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Init.h>

static Arch_Atomic32 s_VectorBitmap[8] = {0xFFFFFFFF};
static Core_Spinlock s_AllocLock;

Interrupt_Handle Interrupt_Allocate(Irql_t irql)
{
    if (irql < IRQL_DIRQL_MIN || irql > IRQL_IPI)
    {
        Log(ERROR, "Interrupt_Allocate: IRQL %u out of allocatable range", irql);
        return INTERRUPT_HANDLE_NULL;
    }

    const u32 base = irql * 16;
    const u32 top  = base + 15;

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 vec = base; vec <= top; vec++)
    {
        if (!Bitmap_TestBit(s_VectorBitmap, vec))
        {
            Bitmap_SetBit(s_VectorBitmap, vec);
            Core_SpinlockRelease(&s_AllocLock);

            Log(TRACE, "allocated vector %u for IRQL %u", vec, irql);
            return vec;
        }
    }

    Core_SpinlockRelease(&s_AllocLock);

    Log(ERROR, "Interrupt_Allocate: no free vectors for IRQL %u (range %u-%u)", irql, base, top);
    return INTERRUPT_HANDLE_NULL;
}

void Interrupt_Free(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256 || vec < 32)
    {
        Log(ERROR, "Interrupt_Free: invalid handle %u", vec);
        return;
    }

    Core_SpinlockAcquire(&s_AllocLock);
    Bitmap_ClearBit(s_VectorBitmap, vec);
    Core_SpinlockRelease(&s_AllocLock);

    Log(TRACE, "freed vector %u", vec);
}

Irql_t Interrupt_GetIrql(Interrupt_Handle handle)
{
    return (Irql_t)((u32)handle >> 4);
}

bool Interrupt_Register(Interrupt_Handle handle, Interrupt_Handler handler, void *context)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    struct Core_SPCB           *spcb = Arch_GetCurrentSpcb();
    Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

    if (reg->Routine != (uptr)Exec_DefaultInterruptHandler)
    {
        Log(ERROR, "vector %u already registered on core %u", vec, spcb->ProcessorNumber);
        return false;
    }

    reg->Context = context;
    Arch_CompilerBarrier();
    reg->Routine = (uptr)handler;

    return true;
}

bool Interrupt_Deregister(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    struct Core_SPCB           *spcb = Arch_GetCurrentSpcb();
    Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

    reg->Routine = (uptr)Exec_DefaultInterruptHandler;
    Arch_CompilerBarrier();
    reg->Context = nullptr;

    return true;
}

bool Interrupt_RegisterGlobal(Interrupt_Handle handle, Interrupt_Handler handler, void *context)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    const u32 cpuCount = Core_GetProcessorCount();

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB *spcb = Core_SpcbGetByNumber(i);
        if (spcb->ArchData.IsrTable[vec].Routine != (uptr)Exec_DefaultInterruptHandler)
        {
            Log(ERROR, "vector %u already registered on core %u, aborting global registration", vec,
                spcb->ProcessorNumber);

            Core_SpinlockRelease(&s_AllocLock);
            return false;
        }
    }

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB           *spcb = Core_SpcbGetByNumber(i);
        Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

        reg->Context = context;
        Arch_CompilerBarrier();
        reg->Routine = (uptr)handler;
    }

    Core_SpinlockRelease(&s_AllocLock);

    Log(TRACE, "globally registered vector %u on %u cores", vec, cpuCount);
    return true;
}

bool Interrupt_DeregisterGlobal(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    const u32 cpuCount = Core_GetProcessorCount();

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB           *spcb = Core_SpcbGetByNumber(i);
        Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

        reg->Routine = (uptr)Exec_DefaultInterruptHandler;
        Arch_CompilerBarrier();
        reg->Context = nullptr;
    }

    Core_SpinlockRelease(&s_AllocLock);
    return true;
}
