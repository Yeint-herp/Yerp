#ifndef SUPERVISOR_CORE_SPINLOCK_H
#define SUPERVISOR_CORE_SPINLOCK_H

#include <arch/Atomic.h>

#define SPINLOCK_FREE   0
#define SPINLOCK_LOCKED 1

typedef struct
{
    Arch_Atomic32 State;
} Core_Spinlock;

bool Core_SpinlockTryAcquire(Core_Spinlock *lock);
void Core_SpinlockAcquire(Core_Spinlock *lock);
void Core_SpinlockRelease(Core_Spinlock *lock);

#endif /* SUPERVISOR_CORE_SPINLOCK_H */
