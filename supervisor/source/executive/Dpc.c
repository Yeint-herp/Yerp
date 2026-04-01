#define DBG_MODULE "Dpc"

#include <arch/CoreLocal.h>
#include <arch/Interrupts.h>
#include <arch/Irql.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Dpc.h>

static Interrupt_Handle s_DpcIpiHandle = INTERRUPT_HANDLE_NULL;

static void s_Drain(void)
{
    struct Core_SPCB *spcb = Arch_GetCurrentSpcb();

    for (;;)
    {
        Core_SpinlockAcquire(&spcb->DpcQueueLock);
        Dpc *dpc = Dsa_RingBufferPop(&spcb->DpcQueue);
        Core_SpinlockRelease(&spcb->DpcQueueLock);

        if (!dpc)
            break;

        dpc->Routine(dpc, dpc->Context, dpc->Arg1, dpc->Arg2);
    }
}

static void s_IpiHandler(Arch_RegisterFrame *frame, void *context)
{
    (void)frame;
    (void)context;

    s_Drain();
    Interrupt_SendEoi(s_DpcIpiHandle);
}

static bool s_Enqueue(struct Core_SPCB *target, Dpc *dpc, bool isSelf)
{
    Irql_t oldIrql = Irql_Raise(IRQL_DISPATCH);
    Core_SpinlockAcquire(&target->DpcQueueLock);

    bool ok = Dsa_RingBufferPush(&target->DpcQueue, dpc);

    Core_SpinlockRelease(&target->DpcQueueLock);

    if (!ok)
    {
        Irql_Lower(oldIrql);
        Log(ERROR, "DPC queue full on processor %u: dropped!", target->ProcessorNumber);
        return false;
    }

    if (isSelf)
        Interrupt_SendIpi(kIpiTargetSelf, 0, s_DpcIpiHandle);
    else
        Interrupt_SendIpi(kIpiTargetSpecific, target->ProcessorNumber, s_DpcIpiHandle);

    Irql_Lower(oldIrql);
    return true;
}

void Dpc_Init(Dpc *dpc, Dpc_Routine routine, void *context)
{
    dpc->Routine = routine;
    dpc->Context = context;
    dpc->Arg1    = nullptr;
    dpc->Arg2    = nullptr;
}

bool Dpc_Queue(Dpc *dpc, void *arg1, void *arg2)
{
    dpc->Arg1 = arg1;
    dpc->Arg2 = arg2;

    return s_Enqueue(Arch_GetCurrentSpcb(), dpc, true);
}

bool Dpc_QueueTarget(Dpc *dpc, void *arg1, void *arg2, u32 processorNumber)
{
    dpc->Arg1 = arg1;
    dpc->Arg2 = arg2;

    struct Core_SPCB *target = Core_SpcbGetByNumber(processorNumber);
    if (!target)
        return false;

    bool isSelf = (target == Arch_GetCurrentSpcb());
    return s_Enqueue(target, dpc, isSelf);
}

void Dpc_SystemInit(void)
{
    s_DpcIpiHandle = Interrupt_Allocate(IRQL_DISPATCH);
    if (s_DpcIpiHandle == INTERRUPT_HANDLE_NULL)
        Panic("failed to allocate DPC IPI vector");

    if (!Interrupt_RegisterGlobal(s_DpcIpiHandle, s_IpiHandler, nullptr))
        Panic("failed to register global DPC IPI handler");

    Log(INFO, "DPC subsystem online (vector handle %u)", s_DpcIpiHandle);
}
