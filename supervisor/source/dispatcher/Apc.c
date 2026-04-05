#include <arch/CoreLocal.h>
#include <arch/Irql.h>
#include <core/Spcb.h>
#include <dispatcher/Dispatcher.h>
#include <dispatcher/Scheduler.h>
#include <dispatcher/Vm.h>

void Ds_ApcInit(Ds_Apc *apc, Ds_ApcRoutine routine, void *context, bool maskable)
{
    Dsa_ListInit(&apc->QueueEntry);
    apc->Routine  = routine;
    apc->Context  = context;
    apc->Arg1     = nullptr;
    apc->Arg2     = nullptr;
    apc->Maskable = maskable;
}

void Ds_ApcQueue(Ds_Apc *apc, Ds_Thread *thread, void *arg1, void *arg2)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    apc->Arg1 = arg1;
    apc->Arg2 = arg2;

    if (apc->Maskable)
        Dsa_ListInsertTail(&thread->ApcMaskableHead, &apc->QueueEntry);
    else
        Dsa_ListInsertTail(&thread->ApcQueueHead, &apc->QueueEntry);

    thread->ApcPending = true;

    if (!apc->Maskable && thread->State == kDsThreadWaiting)
    {
        Dsa_ListRemoveEntry(&thread->WaitBlock.WaitListEntry);

        if (thread->WaitTimerActive)
        {
            Ex_TimerCancel(&thread->WaitTimer);
            thread->WaitTimerActive = false;
        }

        thread->WaitStatus = kDsWaitKernelApc;
        thread->State      = kDsThreadReady;
        Ds_InsertReady(thread);
    }

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_ApcDrainCurrent(void)
{
    Ds_Thread *self = Ds_GetCurrentThread();

    for (;;)
    {
        Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

        Ds_Apc *apc = nullptr;

        if (!Dsa_ListIsEmpty(&self->ApcQueueHead))
        {
            Dsa_ListEntry *e = Dsa_ListRemoveHead(&self->ApcQueueHead);
            apc              = container_of(e, Ds_Apc, QueueEntry);
        }
        else if (!self->ApcMaskableDisable && !Dsa_ListIsEmpty(&self->ApcMaskableHead))
        {
            Dsa_ListEntry *e = Dsa_ListRemoveHead(&self->ApcMaskableHead);
            apc              = container_of(e, Ds_Apc, QueueEntry);
        }

        if (apc == nullptr)
        {
            self->ApcPending = false;
            Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
            break;
        }

        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);

        apc->Routine(apc->Context, apc->Arg1, apc->Arg2);
    }
}

void Ds_ApcDisableMaskable(void)
{
    Ds_GetCurrentThread()->ApcMaskableDisable = true;
}

void Ds_ApcEnableMaskable(void)
{
    Ds_Thread *self          = Ds_GetCurrentThread();
    self->ApcMaskableDisable = false;

    if (self->ApcPending)
        Ds_ApcDrainCurrent();
}
