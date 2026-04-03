#ifndef SUPERVISOR_ARCH_X86_64_LOCALAPIC_H
#define SUPERVISOR_ARCH_X86_64_LOCALAPIC_H

#include <core/Memory.h>

#define LAPIC_REG_ID            0x020
#define LAPIC_REG_VERSION       0x030
#define LAPIC_REG_TPR           0x080
#define LAPIC_REG_EOI           0x0B0
#define LAPIC_REG_SPURIOUS      0x0F0
#define LAPIC_REG_ICR_LO        0x300
#define LAPIC_REG_ICR_HI        0x310
#define LAPIC_REG_LVT_TIMER     0x320
#define LAPIC_REG_LVT_THERMAL   0x330
#define LAPIC_REG_LVT_PERF      0x340
#define LAPIC_REG_LVT_LINT0     0x350
#define LAPIC_REG_LVT_LINT1     0x360
#define LAPIC_REG_LVT_ERROR     0x370
#define LAPIC_REG_TIMER_INIT    0x380
#define LAPIC_REG_TIMER_CURRENT 0x390
#define LAPIC_REG_TIMER_DIVIDE  0x3E0

#define X2APIC_MSR_BASE          0x800
#define X2APIC_MSR_ID            0x802
#define X2APIC_MSR_VERSION       0x803
#define X2APIC_MSR_TPR           0x808
#define X2APIC_MSR_EOI           0x80B
#define X2APIC_MSR_SPURIOUS      0x80F
#define X2APIC_MSR_ICR           0x830
#define X2APIC_MSR_LVT_TIMER     0x832
#define X2APIC_MSR_LVT_THERMAL   0x833
#define X2APIC_MSR_LVT_PERF      0x834
#define X2APIC_MSR_LVT_LINT0     0x835
#define X2APIC_MSR_LVT_LINT1     0x836
#define X2APIC_MSR_LVT_ERROR     0x837
#define X2APIC_MSR_TIMER_INIT    0x838
#define X2APIC_MSR_TIMER_CURRENT 0x839
#define X2APIC_MSR_TIMER_DIVIDE  0x83E

#define LAPIC_SPURIOUS_ENABLE (1u << 8)

#define LAPIC_LVT_MASKED  (1u << 16)
#define LAPIC_LVT_LEVEL   (1u << 15)
#define LAPIC_LVT_PENDING (1u << 12)

#define LAPIC_LVT_DELIV_FIXED  (0u << 8)
#define LAPIC_LVT_DELIV_SMI    (2u << 8)
#define LAPIC_LVT_DELIV_NMI    (4u << 8)
#define LAPIC_LVT_DELIV_INIT   (5u << 8)
#define LAPIC_LVT_DELIV_EXTINT (7u << 8)

#define LAPIC_TIMER_ONESHOT      (0u << 17)
#define LAPIC_TIMER_PERIODIC     (1u << 17)
#define LAPIC_TIMER_TSC_DEADLINE (2u << 17)

#define LAPIC_TIMER_DIV_1   0x0B
#define LAPIC_TIMER_DIV_2   0x00
#define LAPIC_TIMER_DIV_4   0x01
#define LAPIC_TIMER_DIV_8   0x02
#define LAPIC_TIMER_DIV_16  0x03
#define LAPIC_TIMER_DIV_32  0x08
#define LAPIC_TIMER_DIV_64  0x09
#define LAPIC_TIMER_DIV_128 0x0A

typedef struct
{
    u32 TicksPerUs;
    u8  Divider;
} X86_64_LocalApicTimerCal;

void X86_64_LocalApicInit(void);
void X86_64_LocalApicInitAp(void);

void X86_64_LocalApicSendEoi(void);

u32 X86_64_LocalApicGetId(void);

u32  X86_64_LocalApicReadReg(u32 mmioOffset);
void X86_64_LocalApicWriteReg(u32 mmioOffset, u32 value);

void                            X86_64_LocalApicCalibrateTimer(void);
const X86_64_LocalApicTimerCal *X86_64_LocalApicGetTimerCal(void);

void X86_64_LocalApicTimerOneShot(u8 vector, u64 us);

void X86_64_LocalApicTimerPeriodic(u8 vector, u64 us);

void X86_64_LocalApicTimerStop(void);
u32  X86_64_LocalApicTimerReadCurrent(void);

void X86_64_LocalApicTimerOneShotTicks(u8 vector, u32 ticks);

u32 X86_64_LocalApicTimerGetRate(void);

bool X86_64_LocalApicTimerHasTscDeadline(void);
void X86_64_LocalApicTimerTscDeadline(u8 vector, u64 deadline);

#endif /* SUPERVISOR_ARCH_X86_64_LOCALAPIC_H */
