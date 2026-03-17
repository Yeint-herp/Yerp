#include <arch/Atomic.h>

u32 Arch_AtomicExchange32(Arch_Atomic32 *target, u32 value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

u64 Arch_AtomicExchange64(Arch_Atomic64 *target, u64 value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

bool Arch_AtomicCompareExchange32(Arch_Atomic32 *target, u32 expected, u32 desired)
{
    return __atomic_compare_exchange_n(target, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

bool Arch_AtomicCompareExchange64(Arch_Atomic64 *target, u64 expected, u64 desired)
{
    return __atomic_compare_exchange_n(target, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u32 Arch_AtomicLoad32(Arch_Atomic32 *target)
{
    return __atomic_load_n(target, __ATOMIC_ACQUIRE);
}

void Arch_AtomicStore32(Arch_Atomic32 *target, u32 value)
{
    __atomic_store_n(target, value, __ATOMIC_RELEASE);
}

u64 Arch_AtomicLoad64(Arch_Atomic64 *target)
{
    return __atomic_load_n(target, __ATOMIC_ACQUIRE);
}

void Arch_AtomicStore64(Arch_Atomic64 *target, u64 value)
{
    __atomic_store_n(target, value, __ATOMIC_RELEASE);
}
