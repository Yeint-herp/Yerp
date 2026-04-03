#ifndef SUPERVISOR_EXECUTIVE_TIMER_H
#define SUPERVISOR_EXECUTIVE_TIMER_H

#include <arch/Timer.h>
#include <core/Spinlock.h>
#include <dsa/MinHeap.h>
#include <executive/Dpc.h>

#define EX_TAG_TIMER EX_TAG('E', 'x', 'T', 'm')

#define EX_TIMER_WHEEL_LEVELS 4
#define EX_TIMER_WHEEL_BITS   6
#define EX_TIMER_WHEEL_SIZE   (1 << EX_TIMER_WHEEL_BITS)
#define EX_TIMER_WHEEL_MASK   (EX_TIMER_WHEEL_SIZE - 1)

#define EX_TIMER_INSERTED  (1u << 0)
#define EX_TIMER_PERIODIC  (1u << 1)
#define EX_TIMER_HIGH_RES  (1u << 2)
#define EX_TIMER_CANCELLED (1u << 3)

typedef struct Ex_Timer
{
    struct Ex_Timer *Next;
    struct Ex_Timer *Prev;

    u64 Deadline;
    u64 Period;

    u32 Slack;
    u32 Flags;

    u32 Processor;

    Dpc *Dpc;
} Ex_Timer;

typedef struct Ex_TimerCpu
{
    Ex_Timer *Wheel[EX_TIMER_WHEEL_LEVELS][EX_TIMER_WHEEL_SIZE];
    u64       WheelNextExpiry;

    u64 Clock;

    minheap_of(Ex_Timer *) HrHeap;

    Core_Spinlock Lock;
} Ex_TimerCpu;

void Ex_TimerInit(Ex_Timer *timer, Dpc *dpc);
void Ex_TimerInitHighRes(Ex_Timer *timer, Dpc *dpc);

bool Ex_TimerSet(Ex_Timer *timer, u64 deadline, u32 slackTicks);
bool Ex_TimerSetRelative(Ex_Timer *timer, u64 delayTicks, u32 slackTicks);

bool Ex_TimerSetPeriodic(Ex_Timer *timer, u64 period, u32 slackTicks);

bool Ex_TimerCancel(Ex_Timer *timer);

bool Ex_TimerIsPending(Ex_Timer *timer);

u64  Ex_TimerGetNextExpiry(void);
void Ex_TimerCatchUp(u64 now);

bool Ex_TimerSetRemote(Ex_Timer *timer, u64 deadline, u32 slackTicks, u32 targetCpu);

void Ex_TimerSystemInit(void);
void Ex_TimerCpuInit(Ex_TimerCpu *cpu);

u64 Ex_TimerGetTicks(void);

#endif /* SUPERVISOR_EXECUTIVE_TIMER_H */
