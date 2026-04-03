#ifndef SUPERVISOR_ARCH_TIMER_H
#define SUPERVISOR_ARCH_TIMER_H

#include <core/Memory.h>

typedef struct Arch_TimerCal
{
    u64  CounterFreqHz;
    u64  TickPeriodUs;
    u32  HwTicksPerUs;
    bool HasTscDeadline;
} Arch_TimerCal;

void Arch_TimerInit(void);

const Arch_TimerCal *Arch_TimerGetCal(void);

void Arch_TimerOneShotUs(u8 vector, u64 us);
void Arch_TimerOneShotAbsolute(u8 vector, u64 counterDeadline);

void Arch_TimerStop(void);

u64 Arch_TimerReadCounter(void);
u32 Arch_TimerReadRemaining(void);

u64 Arch_TimerUsToCounter(const Arch_TimerCal *cal, u64 us);
u64 Arch_TimerCounterToUs(const Arch_TimerCal *cal, u64 counter);

u64 Arch_TimerTicksToUs(const Arch_TimerCal *cal, u64 ticks);
u64 Arch_TimerUsToTicks(const Arch_TimerCal *cal, u64 us);

u64 Arch_TimerTicksToCounter(const Arch_TimerCal *cal, u64 ticks);
u64 Arch_TimerCounterToTicks(const Arch_TimerCal *cal, u64 counter);

#endif /* SUPERVISOR_ARCH_TIMER_H */
