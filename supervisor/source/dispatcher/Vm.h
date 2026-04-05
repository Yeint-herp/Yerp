#ifndef SUPERVISOR_DISPATCHER_PROCESS_H
#define SUPERVISOR_DISPATCHER_PROCESS_H

#include <arch/Context.h>
#include <core/Spinlock.h>
#include <dispatcher/Dispatcher.h>
#include <dsa/List.h>
#include <executive/Acl.h>
#include <executive/Dpc.h>
#include <executive/Object.h>
#include <executive/Pool.h>
#include <executive/Timer.h>
#include <mm/Vas.h>

#define PS_TAG_THREAD EX_TAG('P', 's', 'T', 'h')
#define PS_TAG_STACK  EX_TAG('P', 's', 'S', 'k')

#define PS_THREAD_STACK_SIZE 0x4000

enum
{
    kDsThreadInitialized,
    kDsThreadReady,
    kDsThreadRunning,
    kDsThreadWaiting,
    kDsThreadTerminated,
};

#define PS_THREAD_SYSTEM (1u << 0)
#define PS_THREAD_IDLE   (1u << 1)

typedef struct Ds_Thread
{
    Ds_DispatcherHeader Header;

    Dsa_ListEntry SchedLink;
    Dsa_ListEntry VmThreadLink;

    Arch_ThreadContext Context;

    struct Ds_Vm *Vm;

    u32 State;
    u32 BasePriority;
    u32 CurrentPriority;
    u32 Quantum;
    u32 QuantumReset;
    u32 IdealProcessor;
    u32 Processor;

    Ds_WaitBlock WaitBlock;
    i32          WaitStatus;
    Ex_Timer     WaitTimer;
    Dpc          WaitTimerDpc;
    bool         WaitTimerActive;

    uptr StackBase;
    uptr StackSize;

    Arch_ThreadEntry EntryPoint;
    void            *EntryParameter;

    u32 Flags;
    u32 ExitCode;
    u32 ThreadId;
} Ds_Thread;

typedef struct Ds_Vm
{
    Ds_DispatcherHeader Header;

    Dsa_ListEntry ThreadListHead;
    Core_Spinlock ThreadListLock;
    u32           ThreadCount;

    Mm_AddressSpace *AddressSpace;
    Ob_HandleTable   HandleTable;

    Acl_Token *Token;

    u32 VmId;
    u32 Flags;
    u32 ExitCode;
} Ds_Vm;

i32      Ds_CreateVm(Acl_Token *token, Ds_Vm **outVm);
void     Ds_DestroyVm(Ds_Vm *process);
Ds_Vm   *Ds_GetSystemVm(void);
Ob_Type *Ds_GetVmType(void);

i32        Ds_CreateThread(Ds_Vm *process, Arch_ThreadEntry entry, void *param, u32 priority, Ds_Thread **outThread);
void       Ds_ReadyThread(Ds_Thread *thread);
void       Ds_ThreadExit(u32 exitCode);
Ds_Thread *Ds_GetCurrentThread(void);
Ob_Type   *Ds_GetThreadType(void);

void Ds_SystemInit(void);

#endif /* SUPERVISOR_DISPATCHER_PROCESS_H */
