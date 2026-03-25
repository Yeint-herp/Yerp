#ifndef SUPERVISOR_ARCH_ATOMIC_H
#define SUPERVISOR_ARCH_ATOMIC_H

typedef volatile u32 Arch_Atomic32;
typedef volatile u64 Arch_Atomic64;

#define Arch_CompilerBarrier() __atomic_signal_fence(__ATOMIC_SEQ_CST)

u32 Arch_AtomicExchange32(Arch_Atomic32 *target, u32 value);
u64 Arch_AtomicExchange64(Arch_Atomic64 *target, u64 value);

bool Arch_AtomicCompareExchange32(Arch_Atomic32 *target, u32 expected, u32 desired);
bool Arch_AtomicCompareExchange64(Arch_Atomic64 *target, u64 expected, u64 desired);

u32 Arch_AtomicAdd32(Arch_Atomic32 *target, u32 value);
u64 Arch_AtomicAdd64(Arch_Atomic64 *target, u64 value);

u32 Arch_AtomicSub32(Arch_Atomic32 *target, u32 value);
u64 Arch_AtomicSub64(Arch_Atomic64 *target, u64 value);

u32  Arch_AtomicLoad32(Arch_Atomic32 *target);
void Arch_AtomicStore32(Arch_Atomic32 *target, u32 value);

u64  Arch_AtomicLoad64(Arch_Atomic64 *target);
void Arch_AtomicStore64(Arch_Atomic64 *target, u64 value);

#endif /* SUPERVISOR_ARCH_ATOMIC_H */
