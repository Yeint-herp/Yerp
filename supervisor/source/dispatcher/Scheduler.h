#ifndef SUPERVISOR_DISPATCHER_SCHEDULER_H
#define SUPERVISOR_DISPATCHER_SCHEDULER_H

#include <dispatcher/Vm.h>
#include <dsa/List.h>

#define DS_PRIORITY_LEVELS     32
#define DS_PRIORITY_IDLE       0
#define DS_PRIORITY_LOWEST     1
#define DS_PRIORITY_NORMAL     8
#define DS_PRIORITY_HIGHEST    15
#define DS_PRIORITY_RT_LOWEST  16
#define DS_PRIORITY_RT_HIGHEST 31

#define DS_QUANTUM_DEFAULT 6
#define DS_QUANTUM_SHORT   2

#define DS_PRIORITY_BOOST_IO     2
#define DS_PRIORITY_BOOST_UNWAIT 1

#define DS_DYNAMIC_PRIORITY_MAX 15

#define DS_TAG_SCHEDULER EX_TAG('K', 'e', 'S', 'c')

typedef struct Ds_SchedulerCpu
{
    Ds_Thread *CurrentThread;
    Ds_Thread *IdleThread;

    Dsa_ListEntry ReadyQueue[DS_PRIORITY_LEVELS];
    u32           ReadyBitmap;

    u32 ProcessorNumber;
    u32 ReadyCount;
} Ds_SchedulerCpu;

void Ds_SchedulerSystemInit(void);
void Ds_SchedulerInitAp();

void       Ds_InsertReady(Ds_Thread *thread);
Ds_Thread *Ds_SelectNextThread(Ds_SchedulerCpu *cpu);

void       Ds_Reschedule(void);
void       Ds_CheckPreemption(Ds_SchedulerCpu *cpu);
Ds_Thread *Ds_PickNextThread(Ds_SchedulerCpu *cpu);

[[noreturn]] void Ds_EnterDispatcher(void);

void Ds_IdleLoop(void *param);
void Ds_ThreadStartup(void *param);

#endif /* SUPERVISOR_DISPATCHER_SCHEDULER_H */
