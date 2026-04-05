#include <arch/CoreLocal.h>
#include <arch/Irql.h>
#include <core/Spcb.h>
#include <dispatcher/Dispatcher.h>
#include <dispatcher/Scheduler.h>
#include <dispatcher/Vm.h>

Core_Spinlock g_DispatcherLock;

void Ds_DispatcherInit(void)
{
    Core_SpinlockInit(&g_DispatcherLock);
}

void Ds_DispatcherHeaderInit(Ds_DispatcherHeader *hdr, u32 type, i32 initialSignal)
{
    hdr->Type        = type;
    hdr->SignalState = initialSignal;
    Dsa_ListInit(&hdr->WaitListHead);
}

static bool s_IsObjectSignaled(Ds_DispatcherHeader *hdr)
{
    switch (hdr->Type)
    {
        case kDsObjectNotificationEvent:
        case kDsObjectSynchronizationEvent:
        case kDsObjectThread:
        case kDsObjectVm:
        case kDsObjectTimer:
            return hdr->SignalState > 0;

        case kDsObjectSemaphore:
            return hdr->SignalState > 0;

        case kDsObjectMutex:
        {
            Ds_Mutex *m = container_of(hdr, Ds_Mutex, Header);
            return hdr->SignalState == 1 || m->Owner == Ds_GetCurrentThread();
        }
        default:
            return false;
    }
}

static void s_AcquireObjectForThread(Ds_DispatcherHeader *hdr, Ds_Thread *thread)
{
    switch (hdr->Type)
    {
        case kDsObjectSynchronizationEvent:
            hdr->SignalState = 0;
            break;

        case kDsObjectSemaphore:
            hdr->SignalState--;
            break;

        case kDsObjectMutex:
        {
            Ds_Mutex *m      = container_of(hdr, Ds_Mutex, Header);
            hdr->SignalState = 0;
            m->Owner         = thread;
            m->RecursionCount++;
            break;
        }
        default:
            break;
    }
}

static void s_UnwaitThreads(Ds_DispatcherHeader *hdr)
{
    while (!Dsa_ListIsEmpty(&hdr->WaitListHead))
    {
        Dsa_ListEntry *entry = hdr->WaitListHead.Flink;
        Ds_WaitBlock  *wb    = container_of(entry, Ds_WaitBlock, WaitListEntry);
        Ds_Thread     *th    = wb->Thread;

        if (!s_IsObjectSignaled(hdr))
            break;

        s_AcquireObjectForThread(hdr, th);

        Dsa_ListRemoveEntry(&wb->WaitListEntry);
        th->WaitStatus = kDsWaitSatisfied;
        th->State      = kDsThreadReady;

        if (th->CurrentPriority <= DS_DYNAMIC_PRIORITY_MAX)
        {
            u32 boosted = th->CurrentPriority + DS_PRIORITY_BOOST_UNWAIT;
            if (boosted > DS_DYNAMIC_PRIORITY_MAX)
                boosted = DS_DYNAMIC_PRIORITY_MAX;
            th->CurrentPriority = boosted;
        }
        th->Quantum = th->QuantumReset;

        if (th->WaitTimerActive)
        {
            Ex_TimerCancel(&th->WaitTimer);
            th->WaitTimerActive = false;
        }

        Ds_InsertReady(th);

        if (hdr->Type == kDsObjectNotificationEvent)
            continue;

        break;
    }
}

static void s_WaitTimeoutDpc(Dpc *dpc, void *context, void *arg1, void *arg2)
{
    (void)dpc;
    (void)arg1;
    (void)arg2;

    Ds_Thread *thread = (Ds_Thread *)context;

    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    if (thread->State == kDsThreadWaiting)
    {
        Dsa_ListRemoveEntry(&thread->WaitBlock.WaitListEntry);
        thread->WaitStatus      = kDsWaitTimeout;
        thread->WaitTimerActive = false;
        thread->State           = kDsThreadReady;
        Ds_InsertReady(thread);
    }

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

i32 Ds_WaitForObject(Ds_DispatcherHeader *object, u64 timeoutTicks)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_Thread       *self = Ds_GetCurrentThread();
    Ds_SchedulerCpu *cpu  = &Arch_GetCurrentSpcb()->Scheduler;

    if (s_IsObjectSignaled(object))
    {
        s_AcquireObjectForThread(object, self);
        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return kDsWaitSatisfied;
    }

    if (timeoutTicks == 0)
    {
        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return kDsWaitTimeout;
    }

    self->WaitBlock.Thread = self;
    self->WaitBlock.Object = object;
    Dsa_ListInsertTail(&object->WaitListHead, &self->WaitBlock.WaitListEntry);

    self->State      = kDsThreadWaiting;
    self->WaitStatus = kDsWaitTimeout;

    if (timeoutTicks != DS_TIMEOUT_INFINITE)
    {
        Dpc_Init(&self->WaitTimerDpc, s_WaitTimeoutDpc, self);
        Ex_TimerInit(&self->WaitTimer, &self->WaitTimerDpc);
        Ex_TimerSetRelative(&self->WaitTimer, timeoutTicks, 0);
        self->WaitTimerActive = true;
    }
    else
        self->WaitTimerActive = false;

    Ds_Thread *next = Ds_PickNextThread(cpu);

    next->State        = kDsThreadRunning;
    cpu->CurrentThread = next;
    next->Processor    = cpu->ProcessorNumber;

    Arch_ContextSwitch(&self->Context, next->Context);

    i32 status = self->WaitStatus;
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
    return status;
}

void Ds_SignalObject(Ds_DispatcherHeader *hdr)
{
    s_UnwaitThreads(hdr);
}

void Ds_EventInit(Ds_Event *ev, u32 type, bool initialState)
{
    Ds_DispatcherHeaderInit(&ev->Header, type, initialState ? 1 : 0);
}

void Ds_EventSet(Ds_Event *ev)
{
    Irql_t old             = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);
    ev->Header.SignalState = 1;
    s_UnwaitThreads(&ev->Header);
    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_EventReset(Ds_Event *ev)
{
    Irql_t old             = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);
    ev->Header.SignalState = 0;
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_EventPulse(Ds_Event *ev)
{
    Irql_t old             = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);
    ev->Header.SignalState = 1;
    s_UnwaitThreads(&ev->Header);
    ev->Header.SignalState = 0;
    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_SemaphoreInit(Ds_Semaphore *sem, i32 initialCount, i32 limit)
{
    Ds_DispatcherHeaderInit(&sem->Header, kDsObjectSemaphore, initialCount);
    sem->Limit = limit;
}

i32 Ds_SemaphoreRelease(Ds_Semaphore *sem, i32 count)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    i32 prev = sem->Header.SignalState;
    i32 next = prev + count;

    if (next > sem->Limit)
    {
        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return -1;
    }

    sem->Header.SignalState = next;
    s_UnwaitThreads(&sem->Header);
    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
    return prev;
}

void Ds_MutexInit(Ds_Mutex *mtx)
{
    Ds_DispatcherHeaderInit(&mtx->Header, kDsObjectMutex, 1);
    mtx->Owner          = nullptr;
    mtx->RecursionCount = 0;
}

void Ds_MutexRelease(Ds_Mutex *mtx)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_Thread *self = Ds_GetCurrentThread();
    if (mtx->Owner != self)
    {
        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return;
    }

    if (--mtx->RecursionCount == 0)
    {
        mtx->Owner              = nullptr;
        mtx->Header.SignalState = 1;
        s_UnwaitThreads(&mtx->Header);
    }

    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}
