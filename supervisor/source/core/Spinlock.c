#include <arch/CpuHint.h>
#include <core/Spinlock.h>

bool Core_SpinlockTryAcquire(Core_Spinlock *lock)
{
    u32 oldValue = Arch_AtomicExchange32(&lock->State, SPINLOCK_LOCKED);
    return (oldValue == SPINLOCK_FREE);
}

void Core_SpinlockAcquire(Core_Spinlock *lock)
{
    for (;;)
    {
        if (Core_SpinlockTryAcquire(lock))
            break;

        while (Arch_AtomicLoad32(&lock->State) == SPINLOCK_LOCKED)
            Arch_CpuRelax();
    }
}

void Core_SpinlockRelease(Core_Spinlock *lock)
{
    Arch_AtomicStore32(&lock->State, SPINLOCK_FREE);
}

void Core_SpinlockInit(Core_Spinlock *lock)
{
    Arch_AtomicStore32(&lock->State, SPINLOCK_FREE);
}

Arch_IrqFlags Core_SpinlockAcquireIrq(Core_Spinlock *lock)
{
    Arch_IrqFlags flags = Arch_IrqSave();
    Core_SpinlockAcquire(lock);
    return flags;
}

void Core_SpinlockReleaseIrq(Core_Spinlock *lock, Arch_IrqFlags flags)
{
    Core_SpinlockRelease(lock);
    Arch_IrqRestore(flags);
}

bool Core_SpinlockTryAcquireIrq(Core_Spinlock *lock, Arch_IrqFlags *flags)
{
    *flags = Arch_IrqSave();
    if (Core_SpinlockTryAcquire(lock))
        return true;

    Arch_IrqRestore(*flags);
    return false;
}

Irql_t Core_SpinlockAcquireIrql(Core_Spinlock *lock, Irql_t irql)
{
    Irql_t old = Irql_Raise(irql);
    Core_SpinlockAcquire(lock);
    return old;
}

void Core_SpinlockReleaseIrql(Core_Spinlock *lock, Irql_t oldIrql)
{
    Core_SpinlockRelease(lock);
    Irql_Lower(oldIrql);
}

bool Core_SpinlockTryAcquireIrql(Core_Spinlock *lock, Irql_t irql, Irql_t *oldIrql)
{
    *oldIrql = Irql_Raise(irql);
    if (Core_SpinlockTryAcquire(lock))
        return true;

    Irql_Lower(*oldIrql);
    return false;
}
