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
