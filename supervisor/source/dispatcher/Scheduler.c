#include <arch/Atomic.h>
#include <arch/CoreLocal.h>
#include <arch/CpuHint.h>
#include <arch/Interrupts.h>
#include <arch/Irq.h>
#include <arch/Irql.h>
#include <arch/Timer.h>
#include <const.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <dispatcher/Dispatcher.h>
#include <dispatcher/Scheduler.h>

#define DS_MIGRATION_THRESHOLD   2
#define s_HighestSetBit(bitmask) (31u - __builtin_clz(bitmask))

static Dpc              s_TickDpc;
static Ex_Timer         s_TickTimer;
static u32              s_ProcessorCount;
static Interrupt_Handle s_KickIpi;

static void s_KickIpiHandler(Arch_RegisterFrame *frame, void *context)
{
    (void)frame;
    (void)context;

    Interrupt_SendEoi(s_KickIpi);

    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);
    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);
    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

static Ds_Thread *s_StealWork(Ds_SchedulerCpu *thief)
{
    Ds_SchedulerCpu *bestVictim = nullptr;
    u32              bestPrio   = 0;

    for (u32 i = 0; i < s_ProcessorCount; i++)
    {
        if (i == thief->ProcessorNumber)
            continue;

        Ds_SchedulerCpu *v = &Core_SpcbGetByNumber(i)->Scheduler;
        if (v->ReadyBitmap == 0)
            continue;

        u32 prio = s_HighestSetBit(v->ReadyBitmap);
        if (bestVictim == nullptr || prio > bestPrio)
        {
            bestPrio   = prio;
            bestVictim = v;
        }
    }

    if (bestVictim == nullptr)
        return nullptr;

    u32            prio  = s_HighestSetBit(bestVictim->ReadyBitmap);
    Dsa_ListEntry *entry = Dsa_ListRemoveHead(&bestVictim->ReadyQueue[prio]);

    if (Dsa_ListIsEmpty(&bestVictim->ReadyQueue[prio]))
        bestVictim->ReadyBitmap &= ~(1u << prio);

    bestVictim->ReadyCount--;

    Ds_Thread *stolen = container_of(entry, Ds_Thread, SchedLink);
    return stolen;
}

static u32 s_FindLeastLoadedCpu(void)
{
    u32 bestCpu  = 0;
    u32 bestLoad = U32_MAX;

    for (u32 i = 0; i < s_ProcessorCount; i++)
    {
        Ds_SchedulerCpu *c = &Core_SpcbGetByNumber(i)->Scheduler;
        if (c->ReadyCount < bestLoad)
        {
            bestLoad = c->ReadyCount;
            bestCpu  = i;
        }
    }
    return bestCpu;
}

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
        Ds_Thread *next = Ds_SelectNextThread(cpu);
        if (next == nullptr)
            next = s_StealWork(cpu);

        if (next != nullptr)
        {
            next->State        = kDsThreadRunning;
            cpu->CurrentThread = next;
            next->Processor    = cpu->ProcessorNumber;
            Arch_ContextSwitch(&current->Context, next->Context);
        }

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
            next = s_StealWork(cpu);
        if (next == nullptr)
            next = cpu->IdleThread;

        next->State        = kDsThreadRunning;
        cpu->CurrentThread = next;
        next->Processor    = cpu->ProcessorNumber;

        if (next != current)
            Arch_ContextSwitch(&current->Context, next->Context);
    }
    else
        Ds_CheckPreemption(cpu);

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_SchedulerSystemInit(void)
{
    s_ProcessorCount = Core_GetProcessorCount();

    Ds_DispatcherInit();
    Ds_SchedulerInitAp();

    s_KickIpi = Interrupt_Allocate(IRQL_DISPATCH);
    Interrupt_RegisterGlobal(s_KickIpi, s_KickIpiHandler, nullptr);

    Dpc_Init(&s_TickDpc, s_TickDpcRoutine, nullptr);
    Ex_TimerInit(&s_TickTimer, &s_TickDpc);
    Ex_TimerSetPeriodic(&s_TickTimer, Arch_TimerUsToTicks(Arch_TimerGetCal(), 10000), 0);
}

void Ds_SchedulerInitAp()
{
    Ds_SchedulerCpu *cpu = &Arch_GetCurrentSpcb()->Scheduler;
    cpu->ProcessorNumber = Arch_GetCurrentSpcb()->ProcessorNumber;
    cpu->ReadyBitmap     = 0;
    cpu->ReadyCount      = 0;

    for (u32 i = 0; i < DS_PRIORITY_LEVELS; i++)
        Dsa_ListInit(&cpu->ReadyQueue[i]);

    Ds_Thread *idle = nullptr;
    Ob_CreateObject(Ds_GetThreadType(), 0, (void **)&idle);

    Ds_DispatcherHeaderInit(&idle->Header, kDsObjectThread, 0);
    Dsa_ListInit(&idle->SchedLink);
    Dsa_ListInit(&idle->VmThreadLink);

    Dsa_ListInit(&idle->ApcQueueHead);
    Dsa_ListInit(&idle->ApcMaskableHead);

    idle->ApcMaskableDisable = false;
    idle->ApcPending         = false;
    idle->Vm                 = Ds_GetSystemVm();
    idle->State              = kDsThreadInitialized;
    idle->BasePriority       = DS_PRIORITY_IDLE;
    idle->CurrentPriority    = DS_PRIORITY_IDLE;
    idle->QuantumReset       = DS_QUANTUM_DEFAULT;
    idle->Quantum            = DS_QUANTUM_DEFAULT;
    idle->Flags              = PS_THREAD_SYSTEM | PS_THREAD_IDLE;
    idle->StackBase          = (uptr)Ex_Allocate(PS_THREAD_STACK_SIZE, PS_TAG_STACK);
    idle->StackSize          = PS_THREAD_STACK_SIZE;
    idle->IdealProcessor     = Arch_GetCurrentSpcb()->ProcessorNumber;
    idle->Processor          = Arch_GetCurrentSpcb()->ProcessorNumber;
    idle->WaitTimerActive    = false;

    uptr idleStackTop = idle->StackBase + PS_THREAD_STACK_SIZE;
    Arch_ContextInit(&idle->Context, idleStackTop, Ds_IdleLoop, nullptr);

    cpu->IdleThread    = idle;
    cpu->CurrentThread = nullptr;
}

void Ds_InsertReady(Ds_Thread *thread)
{
    Ds_SchedulerCpu *ideal  = &Core_SpcbGetByNumber(thread->IdealProcessor)->Scheduler;
    Ds_SchedulerCpu *target = ideal;

    if (s_ProcessorCount > 1)
    {
        u32              leastId = s_FindLeastLoadedCpu();
        Ds_SchedulerCpu *alt     = &Core_SpcbGetByNumber(leastId)->Scheduler;

        if (ideal->ReadyCount >= alt->ReadyCount + DS_MIGRATION_THRESHOLD)
        {
            target                 = alt;
            thread->IdealProcessor = leastId;
        }
    }

    u32 prio = thread->CurrentPriority;
    Dsa_ListInsertTail(&target->ReadyQueue[prio], &thread->SchedLink);
    target->ReadyBitmap |= (1u << prio);
    target->ReadyCount++;

    if (target->ProcessorNumber != Arch_GetCurrentSpcb()->ProcessorNumber)
        Interrupt_SendIpi(kIpiTargetSpecific, target->ProcessorNumber, s_KickIpi);
}

Ds_Thread *Ds_SelectNextThread(Ds_SchedulerCpu *cpu)
{
    if (cpu->ReadyBitmap == 0)
        return nullptr;

    u32            prio  = s_HighestSetBit(cpu->ReadyBitmap);
    Dsa_ListEntry *entry = Dsa_ListRemoveHead(&cpu->ReadyQueue[prio]);

    if (Dsa_ListIsEmpty(&cpu->ReadyQueue[prio]))
        cpu->ReadyBitmap &= ~(1u << prio);

    cpu->ReadyCount--;
    return container_of(entry, Ds_Thread, SchedLink);
}

Ds_Thread *Ds_PickNextThread(Ds_SchedulerCpu *cpu)
{
    Ds_Thread *next = Ds_SelectNextThread(cpu);
    if (next == nullptr)
        next = s_StealWork(cpu);

    if (next == nullptr)
        next = cpu->IdleThread;

    return next;
}

void Ds_Reschedule(void)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_SchedulerCpu *cpu     = &Arch_GetCurrentSpcb()->Scheduler;
    Ds_Thread       *current = cpu->CurrentThread;
    Ds_Thread       *next    = Ds_SelectNextThread(cpu);

    if (next == nullptr)
        next = s_StealWork(cpu);

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

    next->State        = kDsThreadRunning;
    cpu->CurrentThread = next;
    next->Processor    = cpu->ProcessorNumber;

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

        next->State        = kDsThreadRunning;
        cpu->CurrentThread = next;
        next->Processor    = cpu->ProcessorNumber;

        Arch_ContextSwitch(&current->Context, next->Context);
    }
}

void Ds_ThreadStartup(void *param)
{
    Core_SpinlockReleaseIrql(&g_DispatcherLock, IRQL_PASSIVE);

    Ds_ApcDrainCurrent();

    Ds_Thread *self = param;
    self->EntryPoint(self->EntryParameter);

    Ds_ThreadExit(0);
    unreachable();
}

void Ds_IdleLoop(void *param)
{
    (void)param;

    Arch_IrqEnable();
    for (;;)
        Arch_CpuSleep();
}

[[noreturn]] void Ds_EnterDispatcher(void)
{
    Ds_SchedulerCpu *cpu  = &Arch_GetCurrentSpcb()->Scheduler;
    Ds_Thread       *idle = cpu->IdleThread;

    cpu->CurrentThread = idle;
    idle->State        = kDsThreadRunning;
    idle->Processor    = cpu->ProcessorNumber;

    Arch_ThreadContext bootCtx;
    Arch_ContextSwitch(&bootCtx, idle->Context);

    unreachable();
}
