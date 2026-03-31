#define DBG_MODULE "Hpet"

#include <acpi/tables/Hpet.h>
#include <arch/CpuHint.h>
#include <arch/Io.h>
#include <arch/x86_64/Hpet.h>
#include <debug/DbgPrint.h>
#include <mm/Vad.h>
#include <mm/Vas.h>

static X86_64_HpetInfo s_HpetInfo;

u64 X86_64_HpetReadReg(u64 offset)
{
    return Arch_MmioRead64((void *)(s_HpetInfo.MmioBase + offset));
}

void X86_64_HpetWriteReg(u64 offset, u64 value)
{
    Arch_MmioWrite64((void *)(s_HpetInfo.MmioBase + offset), value);
}

bool X86_64_HpetInit(void)
{
    Acpi_Paddr physBase = Acpi_HpetGetBaseAddress();
    if (physBase == 0)
    {
        Log(ERROR, "no HPET base address from ACPI");
        return false;
    }

    uptr mmio = Mm_MapIoSpace(physBase, 0x1000, kMmCacheUncached);
    if (!mmio)
    {
        Log(ERROR, "failed to map HPET at phys %#llx", physBase);
        return false;
    }

    s_HpetInfo.MmioBase = mmio;

    u64 cap = X86_64_HpetReadReg(HPET_REG_CAP_ID);

    s_HpetInfo.PeriodFs      = HPET_CAP_PERIOD_FS(cap);
    s_HpetInfo.NumTimers     = HPET_CAP_NUM_TIMERS(cap);
    s_HpetInfo.Is64Bit       = (cap & HPET_CAP_COUNT_SIZE) != 0;
    s_HpetInfo.LegacyCapable = (cap & HPET_CAP_LEGACY_REPLACE) != 0;

    if (s_HpetInfo.PeriodFs == 0 || s_HpetInfo.PeriodFs > 100000000ULL)
    {
        Log(ERROR, "HPET reports invalid period: %llu fs", s_HpetInfo.PeriodFs);
        return false;
    }

    u64 config = X86_64_HpetReadReg(HPET_REG_CONFIG);
    config &= ~(HPET_CONFIG_ENABLE | HPET_CONFIG_LEGACY);
    X86_64_HpetWriteReg(HPET_REG_CONFIG, config);

    X86_64_HpetWriteReg(HPET_REG_MAIN_COUNTER, 0);

    for (u32 i = 0; i < s_HpetInfo.NumTimers; i++)
    {
        u64 tcfg = X86_64_HpetReadReg(HPET_TIMER_CONFIG(i));
        tcfg &= ~HPET_TIMER_INT_ENABLE;
        X86_64_HpetWriteReg(HPET_TIMER_CONFIG(i), tcfg);
    }

    X86_64_HpetWriteReg(HPET_REG_INT_STATUS, (1ULL << s_HpetInfo.NumTimers) - 1);

    X86_64_HpetEnable();

    Log(INFO, "initialized: %u timers, %u-bit counter, period %llu fs/tick, legacy %s", s_HpetInfo.NumTimers,
        s_HpetInfo.Is64Bit ? 64 : 32, s_HpetInfo.PeriodFs, s_HpetInfo.LegacyCapable ? "capable" : "not capable");

    return true;
}

const X86_64_HpetInfo *X86_64_HpetGetInfo(void)
{
    return &s_HpetInfo;
}

void X86_64_HpetEnable(void)
{
    u64 config = X86_64_HpetReadReg(HPET_REG_CONFIG);
    config |= HPET_CONFIG_ENABLE;
    X86_64_HpetWriteReg(HPET_REG_CONFIG, config);
}

void X86_64_HpetDisable(void)
{
    u64 config = X86_64_HpetReadReg(HPET_REG_CONFIG);
    config &= ~HPET_CONFIG_ENABLE;
    X86_64_HpetWriteReg(HPET_REG_CONFIG, config);
}

u64 X86_64_HpetReadCounter(void)
{
    return X86_64_HpetReadReg(HPET_REG_MAIN_COUNTER);
}

void X86_64_HpetResetCounter(void)
{
    X86_64_HpetDisable();
    X86_64_HpetWriteReg(HPET_REG_MAIN_COUNTER, 0);
    X86_64_HpetEnable();
}

u64 X86_64_HpetTicksToNs(u64 ticks)
{
    unsigned __int128 product = (unsigned __int128)ticks * s_HpetInfo.PeriodFs;
    return product / 1000000ULL;
}

u64 X86_64_HpetNsToTicks(u64 ns)
{
    unsigned __int128 product = (unsigned __int128)ns * 1000000ULL;
    return product / s_HpetInfo.PeriodFs;
}

void X86_64_HpetSpinWaitNs(u64 ns)
{
    u64 target = X86_64_HpetNsToTicks(ns);
    u64 start  = X86_64_HpetReadCounter();

    while ((X86_64_HpetReadCounter() - start) < target)
        Arch_CpuRelax();
}

void X86_64_HpetSpinWaitUs(u64 us)
{
    X86_64_HpetSpinWaitNs(us * 1000ULL);
}

u64 X86_64_HpetReadTimerConfig(u32 timer)
{
    return X86_64_HpetReadReg(HPET_TIMER_CONFIG(timer));
}

void X86_64_HpetWriteTimerConfig(u32 timer, u64 config)
{
    X86_64_HpetWriteReg(HPET_TIMER_CONFIG(timer), config);
}

u64 X86_64_HpetReadTimerComparator(u32 timer)
{
    return X86_64_HpetReadReg(HPET_TIMER_COMPARATOR(timer));
}

void X86_64_HpetWriteTimerComparator(u32 timer, u64 value)
{
    X86_64_HpetWriteReg(HPET_TIMER_COMPARATOR(timer), value);
}

u32 X86_64_HpetTimerRoutingCap(u32 timer)
{
    u64 cfg = X86_64_HpetReadTimerConfig(timer);
    return HPET_TIMER_ROUTE_CAP(cfg);
}

bool X86_64_HpetTimerSupportsPeriodic(u32 timer)
{
    return (X86_64_HpetReadTimerConfig(timer) & HPET_TIMER_PERIODIC_CAP) != 0;
}

bool X86_64_HpetTimerSupports64Bit(u32 timer)
{
    return (X86_64_HpetReadTimerConfig(timer) & HPET_TIMER_64BIT_CAP) != 0;
}

bool X86_64_HpetTimerSupportsFsb(u32 timer)
{
    return (X86_64_HpetReadTimerConfig(timer) & HPET_TIMER_FSB_CAP) != 0;
}
