#define DBG_MODULE "Timer"

#include <arch/CoreLocal.h>
#include <arch/Interrupts.h>
#include <arch/Irql.h>
#include <arch/Timer.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <debug/DbgPrint.h>
#include <executive/Timer.h>

#define WHEEL_SHIFT(level)          ((level) * EX_TIMER_WHEEL_BITS)
#define WHEEL_RANGE(level)          ((u64)EX_TIMER_WHEEL_SIZE << WHEEL_SHIFT(level))
#define WHEEL_SLOT(deadline, level) (((deadline) >> WHEEL_SHIFT(level)) & EX_TIMER_WHEEL_MASK)

#define MAX_IDLE_TICKS 0x100000

static Interrupt_Handle s_ClockVector = INTERRUPT_HANDLE_NULL;

static u64 s_TimerEffectiveNextExpiry(Ex_TimerCpu *cpu)
{
    u64 wheel = cpu->WheelNextExpiry;
    u64 hr    = Dsa_MinHeapEmpty(cpu->HrHeap) ? U64_MAX : Dsa_MinHeapPeek(cpu->HrHeap)->Deadline;

    return (wheel < hr) ? wheel : hr;
}

static i32 s_TimerHeapCmp(Ex_Timer *a, Ex_Timer *b)
{
    if (a->Deadline < b->Deadline)
        return -1;

    if (a->Deadline > b->Deadline)
        return 1;

    return 0;
}

static void s_ListInsertHead(Ex_Timer **head, Ex_Timer *timer)
{
    timer->Prev = nullptr;
    timer->Next = *head;
    if (*head)
        (*head)->Prev = timer;

    *head = timer;
}

static void s_ListRemove(Ex_Timer **head, Ex_Timer *timer)
{
    if (timer->Prev)
        timer->Prev->Next = timer->Next;
    else
        *head = timer->Next;

    if (timer->Next)
        timer->Next->Prev = timer->Prev;

    timer->Next = nullptr;
    timer->Prev = nullptr;
}

static u32 s_WheelLevel(Ex_TimerCpu *cpu, u64 deadline)
{
    u64 delta = deadline - cpu->Clock;

    for (u32 level = 0; level < EX_TIMER_WHEEL_LEVELS; level++)
        if (delta < WHEEL_RANGE(level))
            return level;

    return EX_TIMER_WHEEL_LEVELS - 1;
}

static void s_WheelInsert(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    u32 level = s_WheelLevel(cpu, timer->Deadline);
    u32 slot  = WHEEL_SLOT(timer->Deadline, level);

    s_ListInsertHead(&cpu->Wheel[level][slot], timer);

    timer->Flags |= EX_TIMER_INSERTED;

    if (timer->Deadline < cpu->WheelNextExpiry)
        cpu->WheelNextExpiry = timer->Deadline;
}

static void s_WheelRemove(Ex_Timer *timer)
{
    Ex_TimerCpu *cpu   = &Core_SpcbGetByNumber(timer->Processor)->Timers;
    u32          level = s_WheelLevel(cpu, timer->Deadline);
    u32          slot  = WHEEL_SLOT(timer->Deadline, level);

    s_ListRemove(&cpu->Wheel[level][slot], timer);

    timer->Flags &= ~EX_TIMER_INSERTED;
}

static void s_UpdateWheelNextExpiry(Ex_TimerCpu *cpu)
{
    cpu->WheelNextExpiry = U64_MAX;

    for (u32 slot = 0; slot < EX_TIMER_WHEEL_SIZE; slot++)
        for (Ex_Timer *t = cpu->Wheel[0][slot]; t; t = t->Next)
            if (t->Deadline < cpu->WheelNextExpiry)
                cpu->WheelNextExpiry = t->Deadline;

    if (cpu->WheelNextExpiry != U64_MAX)
        return;

    for (u32 level = 1; level < EX_TIMER_WHEEL_LEVELS; level++)
    {
        for (u32 slot = 0; slot < EX_TIMER_WHEEL_SIZE; slot++)
            if (cpu->Wheel[level][slot])
                for (Ex_Timer *t = cpu->Wheel[level][slot]; t; t = t->Next)
                    if (t->Deadline < cpu->WheelNextExpiry)
                        cpu->WheelNextExpiry = t->Deadline;

        if (cpu->WheelNextExpiry != U64_MAX)
            return;
    }
}

static void s_CascadeLevel(Ex_TimerCpu *cpu, u32 level)
{
    u32       slot = WHEEL_SLOT(cpu->Clock, level);
    Ex_Timer *list = cpu->Wheel[level][slot];

    cpu->Wheel[level][slot] = nullptr;

    while (list)
    {
        Ex_Timer *next = list->Next;
        list->Next     = nullptr;
        list->Prev     = nullptr;
        list->Flags &= ~EX_TIMER_INSERTED;

        s_WheelInsert(cpu, list);

        list = next;
    }
}

static void s_HrInsert(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    Dsa_MinHeapPush(cpu->HrHeap, timer, s_TimerHeapCmp, EX_TAG_TIMER);
    timer->Flags |= EX_TIMER_INSERTED;
}

static void s_HrRemove(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    usize count = Dsa_MinHeapCount(cpu->HrHeap);

    for (usize i = 0; i < count; i++)
    {
        if (cpu->HrHeap[i] == timer)
        {
            Dsa_MinHeapRemoveAt(cpu->HrHeap, i, s_TimerHeapCmp);
            timer->Flags &= ~EX_TIMER_INSERTED;
            return;
        }
    }
}

static Ex_TimerCpu *s_GetLocalTimerCpu(void)
{
    return &Arch_GetCurrentSpcb()->Timers;
}

static Ex_TimerCpu *s_GetTimerCpu(u32 processorNumber)
{
    return &Core_SpcbGetByNumber(processorNumber)->Timers;
}

static void s_RemoveTimer(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    if (!(timer->Flags & EX_TIMER_INSERTED))
        return;

    if (timer->Flags & EX_TIMER_HIGH_RES)
        s_HrRemove(cpu, timer);
    else
        s_WheelRemove(timer);
}

static void s_InsertTimer(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    if (timer->Flags & EX_TIMER_HIGH_RES)
        s_HrInsert(cpu, timer);
    else
        s_WheelInsert(cpu, timer);
}

static void s_FireTimer(Ex_TimerCpu *cpu, Ex_Timer *timer)
{
    timer->Flags &= ~EX_TIMER_INSERTED;

    if (timer->Dpc)
        Dpc_QueueTarget(timer->Dpc, nullptr, nullptr, timer->Processor);

    if ((timer->Flags & EX_TIMER_PERIODIC) && timer->Period > 0)
    {
        timer->Deadline += timer->Period;
        s_InsertTimer(cpu, timer);
    }
}

static void s_ProcessWheelTick(Ex_TimerCpu *cpu)
{
    cpu->Clock++;

    for (u32 level = 1; level < EX_TIMER_WHEEL_LEVELS; level++)
    {
        u64 cascade_mask = (1ULL << WHEEL_SHIFT(level)) - 1;

        if ((cpu->Clock & cascade_mask) == 0)
            s_CascadeLevel(cpu, level);
        else
            break;
    }

    u32       slot = WHEEL_SLOT(cpu->Clock, 0);
    Ex_Timer *list = cpu->Wheel[0][slot];

    cpu->Wheel[0][slot] = nullptr;

    while (list)
    {
        Ex_Timer *next = list->Next;
        list->Next     = nullptr;
        list->Prev     = nullptr;

        s_FireTimer(cpu, list);

        list = next;
    }
}

static void s_ProcessHrTimers(Ex_TimerCpu *cpu)
{
    u64 now = Arch_TimerReadCounter();

    while (!Dsa_MinHeapEmpty(cpu->HrHeap))
    {
        Ex_Timer *earliest = Dsa_MinHeapPeek(cpu->HrHeap);

        if (earliest->Flags & EX_TIMER_CANCELLED)
        {
            Dsa_MinHeapPop(cpu->HrHeap, s_TimerHeapCmp);
            earliest->Flags &= ~(EX_TIMER_INSERTED | EX_TIMER_CANCELLED);
            continue;
        }

        if (earliest->Deadline > now)
            break;

        Dsa_MinHeapPop(cpu->HrHeap, s_TimerHeapCmp);
        s_FireTimer(cpu, earliest);
    }
}

void s_TimerReprogram(void)
{
    Ex_TimerCpu         *cpu = s_GetLocalTimerCpu();
    const Arch_TimerCal *cal = Arch_TimerGetCal();

    u64 wheelUs = cal->TickPeriodUs;

    u64 hrUs = U64_MAX;
    if (!Dsa_MinHeapEmpty(cpu->HrHeap))
    {
        u64 now  = Arch_TimerReadCounter();
        u64 hrDl = Dsa_MinHeapPeek(cpu->HrHeap)->Deadline;
        if (hrDl <= now)
            hrUs = 0;
        else
            hrUs = Arch_TimerCounterToUs(cal, hrDl - now);
    }

    u64 us = (hrUs < wheelUs) ? hrUs : wheelUs;

    if (us == 0)
        us = 1;

    Arch_TimerOneShotUs(s_ClockVector, us);
}

static void s_ClockIsr(Arch_RegisterFrame *, void *)
{
    Interrupt_SendEoi(s_ClockVector);
    Ex_TimerCpu *cpu = s_GetLocalTimerCpu();

    Irql_t oldIrql = Core_SpinlockAcquireIrql(&cpu->Lock, IRQL_CLOCK);

    s_ProcessWheelTick(cpu);
    s_ProcessHrTimers(cpu);
    s_UpdateWheelNextExpiry(cpu);

    Core_SpinlockReleaseIrql(&cpu->Lock, oldIrql);

    s_TimerReprogram();
}

void Ex_TimerInit(Ex_Timer *timer, Dpc *dpc)
{
    Core_ZeroMemory(timer, sizeof *timer);
    timer->Dpc = dpc;
}

void Ex_TimerInitHighRes(Ex_Timer *timer, Dpc *dpc)
{
    Core_ZeroMemory(timer, sizeof *timer);
    timer->Dpc   = dpc;
    timer->Flags = EX_TIMER_HIGH_RES;
}

bool Ex_TimerSet(Ex_Timer *timer, u64 deadline, u32 slackTicks)
{
    Ex_TimerCpu *cpu     = s_GetTimerCpu(timer->Processor);
    Irql_t       oldIrql = Core_SpinlockAcquireIrql(&cpu->Lock, IRQL_CLOCK);

    bool wasPending = (timer->Flags & EX_TIMER_INSERTED) != 0;
    if (wasPending)
        s_RemoveTimer(cpu, timer);

    timer->Deadline = deadline;
    timer->Slack    = slackTicks;
    timer->Flags &= ~EX_TIMER_CANCELLED;

    s_InsertTimer(cpu, timer);

    Core_SpinlockReleaseIrql(&cpu->Lock, oldIrql);

    s_TimerReprogram();

    return wasPending;
}

bool Ex_TimerSetRelative(Ex_Timer *timer, u64 delayTicks, u32 slackTicks)
{
    const Arch_TimerCal *cal = Arch_TimerGetCal();
    u64                  deadline;

    if (timer->Flags & EX_TIMER_HIGH_RES)
    {
        u64 delayCounter = Arch_TimerTicksToCounter(cal, delayTicks);
        deadline         = Arch_TimerReadCounter() + delayCounter;
    }
    else
        deadline = s_GetTimerCpu(timer->Processor)->Clock + delayTicks;

    return Ex_TimerSet(timer, deadline, slackTicks);
}

bool Ex_TimerSetPeriodic(Ex_Timer *timer, u64 period, u32 slackTicks)
{
    const Arch_TimerCal *cal = Arch_TimerGetCal();

    timer->Period = period;
    timer->Flags |= EX_TIMER_PERIODIC;

    u64 deadline;

    if (timer->Flags & EX_TIMER_HIGH_RES)
    {
        u64 periodCounter = Arch_TimerTicksToCounter(cal, period);
        timer->Period     = periodCounter;
        deadline          = Arch_TimerReadCounter() + periodCounter;
    }
    else
    {
        timer->Period = period;
        deadline      = s_GetTimerCpu(timer->Processor)->Clock + period;
    }

    return Ex_TimerSet(timer, deadline, slackTicks);
}

bool Ex_TimerCancel(Ex_Timer *timer)
{
    Ex_TimerCpu *cpu     = s_GetTimerCpu(timer->Processor);
    Irql_t       oldIrql = Core_SpinlockAcquireIrql(&cpu->Lock, IRQL_CLOCK);

    bool wasPending = (timer->Flags & EX_TIMER_INSERTED) != 0;

    if (wasPending)
    {
        s_RemoveTimer(cpu, timer);
        s_UpdateWheelNextExpiry(cpu);
    }

    timer->Flags &= ~EX_TIMER_PERIODIC;

    Core_SpinlockReleaseIrql(&cpu->Lock, oldIrql);

    if (wasPending)
        s_TimerReprogram();

    return wasPending;
}

u64 Ex_TimerGetNextExpiry(void)
{
    Ex_TimerCpu *cpu = s_GetLocalTimerCpu();

    return s_TimerEffectiveNextExpiry(cpu);
}

void Ex_TimerCatchUp(u64 now)
{
    Ex_TimerCpu *cpu     = s_GetLocalTimerCpu();
    Irql_t       oldIrql = Core_SpinlockAcquireIrql(&cpu->Lock, IRQL_CLOCK);

    while (cpu->Clock < now)
        s_ProcessWheelTick(cpu);

    s_ProcessHrTimers(cpu);
    s_UpdateWheelNextExpiry(cpu);

    Core_SpinlockReleaseIrql(&cpu->Lock, oldIrql);

    s_TimerReprogram();
}

bool Ex_TimerSetRemote(Ex_Timer *timer, u64 deadline, u32 slackTicks, u32 targetCpu)
{
    timer->Processor = targetCpu;

    Ex_TimerCpu *cpu     = s_GetTimerCpu(targetCpu);
    Irql_t       oldIrql = Core_SpinlockAcquireIrql(&cpu->Lock, IRQL_CLOCK);

    bool wasPending = (timer->Flags & EX_TIMER_INSERTED) != 0;
    if (wasPending)
        s_RemoveTimer(cpu, timer);

    timer->Deadline = deadline;
    timer->Slack    = slackTicks;
    timer->Flags &= ~EX_TIMER_CANCELLED;

    s_InsertTimer(cpu, timer);

    bool needIpi = (deadline < s_TimerEffectiveNextExpiry(cpu));

    Core_SpinlockReleaseIrql(&cpu->Lock, oldIrql);

    if (needIpi)
        Interrupt_SendIpi(kIpiTargetSpecific, targetCpu, s_ClockVector);

    return wasPending;
}

void Ex_TimerCpuInit(Ex_TimerCpu *cpu)
{
    Core_SpinlockInit(&cpu->Lock);
    cpu->WheelNextExpiry = U64_MAX;
}

u64 Ex_TimerGetTicks(void)
{
    return s_GetLocalTimerCpu()->Clock;
}

void Ex_TimerSystemInit(void)
{
    Arch_TimerInit();

    s_ClockVector = Interrupt_Allocate(IRQL_CLOCK);
    if (s_ClockVector == INTERRUPT_HANDLE_NULL)
    {
        Log(ERROR, "failed to allocate clock interrupt vector");
        return;
    }
    Interrupt_RegisterGlobal(s_ClockVector, s_ClockIsr, nullptr);

    Ex_TimerCpuInit(&Arch_GetCurrentSpcb()->Timers);

    Log(INFO, "timer subsystem initialized");
}
