#ifndef SUPERVISOR_ARCH_X86_64_HPET_H
#define SUPERVISOR_ARCH_X86_64_HPET_H

#include <core/Memory.h>

#define HPET_REG_CAP_ID       0x000
#define HPET_REG_CONFIG       0x010
#define HPET_REG_INT_STATUS   0x020
#define HPET_REG_MAIN_COUNTER 0x0F0

#define HPET_TIMER_REG(n, off)   (0x100 + 0x20 * (n) + (off))
#define HPET_TIMER_CONFIG(n)     HPET_TIMER_REG(n, 0x00)
#define HPET_TIMER_COMPARATOR(n) HPET_TIMER_REG(n, 0x08)
#define HPET_TIMER_FSB_ROUTE(n)  HPET_TIMER_REG(n, 0x10)

#define HPET_CAP_COUNT_SIZE     (1ULL << 13)
#define HPET_CAP_LEGACY_REPLACE (1ULL << 15)

#define HPET_CAP_NUM_TIMERS(cap) ((((cap) >> 8) & 0x1F) + 1)
#define HPET_CAP_PERIOD_FS(cap)  ((cap) >> 32)

#define HPET_CONFIG_ENABLE (1ULL << 0)
#define HPET_CONFIG_LEGACY (1ULL << 1)

#define HPET_TIMER_INT_ENABLE   (1ULL << 2)
#define HPET_TIMER_PERIODIC     (1ULL << 3)
#define HPET_TIMER_PERIODIC_CAP (1ULL << 4)
#define HPET_TIMER_64BIT_CAP    (1ULL << 5)
#define HPET_TIMER_SET_ACCUM    (1ULL << 6)
#define HPET_TIMER_32BIT_FORCE  (1ULL << 8)
#define HPET_TIMER_FSB_ENABLE   (1ULL << 14)
#define HPET_TIMER_FSB_CAP      (1ULL << 15)

#define HPET_TIMER_INT_ROUTE_MASK  0x3E00ULL
#define HPET_TIMER_INT_ROUTE_SHIFT 9
#define HPET_TIMER_ROUTE_CAP(cfg)  ((u32)((cfg) >> 32))

typedef struct
{
    uptr MmioBase;
    u64  PeriodFs;
    u32  NumTimers;
    bool Is64Bit;
    bool LegacyCapable;
} X86_64_HpetInfo;

bool X86_64_HpetInit(void);

const X86_64_HpetInfo *X86_64_HpetGetInfo(void);

u64  X86_64_HpetReadReg(u64 offset);
void X86_64_HpetWriteReg(u64 offset, u64 value);

void X86_64_HpetEnable(void);
void X86_64_HpetDisable(void);

u64  X86_64_HpetReadCounter(void);
void X86_64_HpetResetCounter(void);

u64 X86_64_HpetTicksToNs(u64 ticks);
u64 X86_64_HpetNsToTicks(u64 ns);

void X86_64_HpetSpinWaitNs(u64 ns);
void X86_64_HpetSpinWaitUs(u64 us);

u64  X86_64_HpetReadTimerConfig(u32 timer);
void X86_64_HpetWriteTimerConfig(u32 timer, u64 config);

u64  X86_64_HpetReadTimerComparator(u32 timer);
void X86_64_HpetWriteTimerComparator(u32 timer, u64 value);

u32 X86_64_HpetTimerRoutingCap(u32 timer);

bool X86_64_HpetTimerSupportsPeriodic(u32 timer);
bool X86_64_HpetTimerSupports64Bit(u32 timer);
bool X86_64_HpetTimerSupportsFsb(u32 timer);

#endif /* SUPERVISOR_ARCH_X86_64_HPET_H */
