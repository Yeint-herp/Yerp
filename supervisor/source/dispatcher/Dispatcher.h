#ifndef SUPERVISOR_DISPATCHER_DISPATCHER_H
#define SUPERVISOR_DISPATCHER_DISPATCHER_H

#include <core/Spinlock.h>
#include <dsa/List.h>

enum
{
    kDsObjectNotificationEvent,
    kDsObjectSynchronizationEvent,
    kDsObjectSemaphore,
    kDsObjectMutex,
    kDsObjectThread,
    kDsObjectVm,
    kDsObjectTimer,
};

enum
{
    kDsWaitSatisfied = 0,
    kDsWaitTimeout,
    kDsWaitAbandoned,
};

#define DS_TIMEOUT_INFINITE ((u64) - 1)

typedef struct Ds_DispatcherHeader
{
    u32           Type;
    i32           SignalState;
    Dsa_ListEntry WaitListHead;
} Ds_DispatcherHeader;

void Ds_DispatcherHeaderInit(Ds_DispatcherHeader *hdr, u32 type, i32 initialSignal);

typedef struct Ds_WaitBlock
{
    Dsa_ListEntry        WaitListEntry;
    struct Ds_Thread    *Thread;
    Ds_DispatcherHeader *Object;
} Ds_WaitBlock;

typedef struct Ds_Event
{
    Ds_DispatcherHeader Header;
} Ds_Event;

typedef struct Ds_Semaphore
{
    Ds_DispatcherHeader Header;
    i32                 Limit;
} Ds_Semaphore;

typedef struct Ds_Mutex
{
    Ds_DispatcherHeader Header;
    struct Ds_Thread   *Owner;
    u32                 RecursionCount;
} Ds_Mutex;

void Ds_EventInit(Ds_Event *event, u32 type, bool initialState);
void Ds_EventSet(Ds_Event *event);
void Ds_EventReset(Ds_Event *event);
void Ds_EventPulse(Ds_Event *event);

void Ds_SemaphoreInit(Ds_Semaphore *sem, i32 initialCount, i32 limit);
i32  Ds_SemaphoreRelease(Ds_Semaphore *sem, i32 count);

void Ds_MutexInit(Ds_Mutex *mtx);
void Ds_MutexRelease(Ds_Mutex *mtx);

i32 Ds_WaitForObject(Ds_DispatcherHeader *object, u64 timeoutTicks);

void Ds_SignalObject(Ds_DispatcherHeader *hdr);

extern Core_Spinlock g_DispatcherLock;

void Ds_DispatcherInit(void);

#endif /* SUPERVISOR_DISPATCHER_DISPATCHER_H */
