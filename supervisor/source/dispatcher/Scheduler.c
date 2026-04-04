#include <arch/Atomic.h>
#include <arch/CoreLocal.h>
#include <arch/CpuHint.h>
#include <arch/Irq.h>
#include <arch/Irql.h>
#include <arch/Timer.h>
#include <const.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <dispatcher/Dispatcher.h>
#include <dispatcher/Scheduler.h>

static Dpc      s_TickDpc;
static Ex_Timer s_TickTimer;

static void s_TickDpcRoutine(Dpc *dpc, void *context, void *arg1, void *arg2)
{
    (void)dpc;
    (void)context;
    (void)arg1;
    (void)arg2;
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_SchedulerCpu *cpu     = &Arch_GetCurrentSpcb()->Scheduler;
    Ds_Thread       *current = cpu->CurrentThread;

    if (current->Flags & PS_THREAD_IDLE)
    {
        Ds_CheckPreemption(cpu);
        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return;
    }

    if (--current->Quantum == 0)
    {
        current->Quantum = current->QuantumReset;

        if (current->CurrentPriority <= DS_DYNAMIC_PRIORITY_MAX && current->CurrentPriority > current->BasePriority)
            current->CurrentPriority--;

        current->State = kDsThreadReady;
        Ds_InsertReady(current);

        Ds_Thread *next = Ds_SelectNextThread(cpu);
        if (next == nullptr)
            next = cpu->IdleThread;

        next->State          = kDsThreadRunning;
        cpu->CurrentThread   = next;
        next->IdealProcessor = cpu->ProcessorNumber;

        if (next != current)
            Arch_ContextSwitch(&current->Context, next->Context);
    }
    else
        Ds_CheckPreemption(cpu);

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_SchedulerSystemInit(void)
{
    Ds_DispatcherInit();
    Ds_SchedulerInitAp();

    Dpc_Init(&s_TickDpc, s_TickDpcRoutine, nullptr);
    Ex_TimerInit(&s_TickTimer, &s_TickDpc);
    Ex_TimerSetPeriodic(&s_TickTimer, Arch_TimerUsToTicks(Arch_TimerGetCal(), 10000), 0);
}

#define s_HighestSetBit(bitmask) (31u - __builtin_clz(bitmask))

void Ds_SchedulerInitAp()
{
    Ds_SchedulerCpu *cpu = &Arch_GetCurrentSpcb()->Scheduler;
    cpu->ProcessorNumber = Arch_GetCurrentSpcb()->ProcessorNumber;
    cpu->ReadyBitmap     = 0;

    for (u32 i = 0; i < DS_PRIORITY_LEVELS; i++)
        Dsa_ListInit(&cpu->ReadyQueue[i]);

    Ds_Thread *idle = nullptr;
    Ob_CreateObject(Ds_GetThreadType(), 0, (void **)&idle);

    Ds_DispatcherHeaderInit(&idle->Header, kDsObjectThread, 0);
    Dsa_ListInit(&idle->SchedLink);
    Dsa_ListInit(&idle->VmThreadLink);

    idle->Vm              = Ds_GetSystemVm();
    idle->State           = kDsThreadInitialized;
    idle->BasePriority    = DS_PRIORITY_IDLE;
    idle->CurrentPriority = DS_PRIORITY_IDLE;
    idle->QuantumReset    = DS_QUANTUM_DEFAULT;
    idle->Quantum         = DS_QUANTUM_DEFAULT;
    idle->Flags           = PS_THREAD_SYSTEM | PS_THREAD_IDLE;
    idle->StackBase       = 0;
    idle->StackSize       = 0;
    idle->IdealProcessor  = Arch_GetCurrentSpcb()->ProcessorNumber;
    idle->WaitTimerActive = false;

    cpu->IdleThread    = idle;
    cpu->CurrentThread = nullptr;
}

void Ds_InsertReady(Ds_Thread *thread)
{
    // TODO load balancing
    Ds_SchedulerCpu *cpu  = &Core_SpcbGetByNumber(thread->IdealProcessor)->Scheduler;
    u32              prio = thread->CurrentPriority;

    Dsa_ListInsertTail(&cpu->ReadyQueue[prio], &thread->SchedLink);
    cpu->ReadyBitmap |= (1u << prio);
}

Ds_Thread *Ds_SelectNextThread(Ds_SchedulerCpu *cpu)
{
    if (cpu->ReadyBitmap == 0)
        return nullptr;

    u32            prio  = s_HighestSetBit(cpu->ReadyBitmap);
    Dsa_ListEntry *entry = Dsa_ListRemoveHead(&cpu->ReadyQueue[prio]);

    if (Dsa_ListIsEmpty(&cpu->ReadyQueue[prio]))
        cpu->ReadyBitmap &= ~(1u << prio);

    return container_of(entry, Ds_Thread, SchedLink);
}

void Ds_Reschedule(void)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_SchedulerCpu *cpu     = &Arch_GetCurrentSpcb()->Scheduler;
    Ds_Thread       *current = cpu->CurrentThread;
    Ds_Thread       *next    = Ds_SelectNextThread(cpu);

    if (next == nullptr || next == current)
    {
        if (next == current)
            Ds_InsertReady(next);

        Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
        return;
    }

    if (current->State == kDsThreadRunning)
    {
        current->State = kDsThreadReady;
        Ds_InsertReady(current);
    }

    next->State          = kDsThreadRunning;
    cpu->CurrentThread   = next;
    next->IdealProcessor = cpu->ProcessorNumber;

    Arch_ContextSwitch(&current->Context, next->Context);

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_CheckPreemption(Ds_SchedulerCpu *cpu)
{
    if (cpu->ReadyBitmap == 0)
        return;

    if (cpu->CurrentThread == nullptr)
        return;

    u32 highestReady = s_HighestSetBit(cpu->ReadyBitmap);
    u32 currentPrio  = cpu->CurrentThread->CurrentPriority;

    if (highestReady > currentPrio)
    {
        Ds_Thread *current = cpu->CurrentThread;
        Ds_Thread *next    = Ds_SelectNextThread(cpu);

        if (current->State == kDsThreadRunning)
        {
            current->State = kDsThreadReady;
            Ds_InsertReady(current);
        }

        next->State          = kDsThreadRunning;
        cpu->CurrentThread   = next;
        next->IdealProcessor = cpu->ProcessorNumber;

        Arch_ContextSwitch(&current->Context, next->Context);
    }
}

void Ds_ThreadStartup(void *param)
{
    Core_SpinlockReleaseIrql(&g_DispatcherLock, IRQL_PASSIVE);

    Ds_Thread *self = param;
    self->EntryPoint(self->EntryParameter);

    Ds_ThreadExit(0);
    unreachable();
}

_Noreturn void Ds_EnterDispatcher(void)
{
    Ds_SchedulerCpu *cpu  = &Arch_GetCurrentSpcb()->Scheduler;
    Ds_Thread       *idle = cpu->IdleThread;

    cpu->CurrentThread   = idle;
    idle->State          = kDsThreadRunning;
    idle->IdealProcessor = cpu->ProcessorNumber;

    Ds_Reschedule();

    Arch_IrqEnable();
    for (;;)
        Arch_CpuSleep();
}
