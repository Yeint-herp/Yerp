#define DBG_MODULE "ArchTimer"

#include <arch/Timer.h>
#include <arch/x86_64/LocalApic.h>
#include <arch/x86_64/Tsc.h>
#include <debug/DbgPrint.h>

#define TIMER_TICK_PERIOD_US 1000

static Arch_TimerCal s_Cal;

void Arch_TimerInit(void)
{
    const X86_64_TscInfo *tsc = X86_64_TscGetInfo();

    if (!tsc->IsAvailable)
        Log(WARN, "TSC unavailable");

    s_Cal.HwTicksPerUs   = X86_64_LocalApicTimerGetRate();
    s_Cal.TickPeriodUs   = TIMER_TICK_PERIOD_US;
    s_Cal.CounterFreqHz  = tsc->IsAvailable ? tsc->FreqHz : 0;
    s_Cal.HasTscDeadline = X86_64_LocalApicTimerHasTscDeadline() && tsc->IsAvailable && tsc->IsInvariant;

    Log(INFO, "TSC-deadline %s", s_Cal.HasTscDeadline ? "yes" : "no");
}

const Arch_TimerCal *Arch_TimerGetCal(void)
{
    return &s_Cal;
}

void Arch_TimerOneShotUs(u8 vector, u64 us)
{
    u64 hwTicks = us * s_Cal.HwTicksPerUs;

    if (hwTicks > 0xFFFFFFFFULL)
        hwTicks = 0xFFFFFFFFULL;
    if (hwTicks == 0)
        hwTicks = 1;

    X86_64_LocalApicTimerOneShotTicks(vector, hwTicks);
}

void Arch_TimerOneShotAbsolute(u8 vector, u64 counterDeadline)
{
    if (s_Cal.HasTscDeadline)
    {
        X86_64_LocalApicTimerTscDeadline(vector, counterDeadline);
        return;
    }

    const u64 now = X86_64_TscRead();
    u64       delta;

    if (counterDeadline > now)
        delta = counterDeadline - now;
    else
        delta = 1;

    const X86_64_TscInfo *tsc = X86_64_TscGetInfo();
    u64                   us  = X86_64_TscTicksToUs(tsc, delta);

    if (us == 0)
        us = 1;

    Arch_TimerOneShotUs(vector, us);
}

void Arch_TimerStop(void)
{
    X86_64_LocalApicTimerStop();
}

u64 Arch_TimerReadCounter(void)
{
    return X86_64_TscRead();
}

u32 Arch_TimerReadRemaining(void)
{
    return X86_64_LocalApicTimerReadCurrent();
}

u64 Arch_TimerUsToCounter(const Arch_TimerCal *cal, u64 us)
{
    return us * (cal->CounterFreqHz / 1000000);
}

u64 Arch_TimerCounterToUs(const Arch_TimerCal *cal, u64 counter)
{
    return counter / (cal->CounterFreqHz / 1000000);
}

u64 Arch_TimerTicksToUs(const Arch_TimerCal *cal, u64 ticks)
{
    return ticks * cal->TickPeriodUs;
}

u64 Arch_TimerUsToTicks(const Arch_TimerCal *cal, u64 us)
{
    return us / cal->TickPeriodUs;
}

u64 Arch_TimerTicksToCounter(const Arch_TimerCal *cal, u64 ticks)
{
    return Arch_TimerUsToCounter(cal, Arch_TimerTicksToUs(cal, ticks));
}

u64 Arch_TimerCounterToTicks(const Arch_TimerCal *cal, u64 counter)
{
    return Arch_TimerUsToTicks(cal, Arch_TimerCounterToUs(cal, counter));
}
