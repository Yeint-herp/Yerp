#include <arch/Atomic.h>
#include <arch/CoreLocal.h>
#include <arch/Irql.h>
#include <const.h>
#include <core/Memory.h>
#include <core/Spcb.h>
#include <dispatcher/Scheduler.h>
#include <dispatcher/Vm.h>

static Ob_Type *s_VmType;
static Ob_Type *s_ThreadType;

static Ds_Vm *s_SystemVm;

static Arch_Atomic32 s_NextVmId;
static Arch_Atomic32 s_NextThreadId;

static void s_VmDeleteProc(void *object);
static void s_ThreadDeleteProc(void *object);

static Ob_TypeInfo s_VmInfo = {
    .Name            = "Vm",
    .ObjectBodySize  = sizeof(Ds_Vm),
    .DeleteProcedure = s_VmDeleteProc,
    .CloseProcedure  = nullptr,
    .PoolTag         = EX_TAG_PROCESS,
    .ValidAccessMask = 0xFFFFFFFF,
};

static Ob_TypeInfo s_ThreadInfo = {
    .Name            = "Thread",
    .ObjectBodySize  = sizeof(Ds_Thread),
    .DeleteProcedure = s_ThreadDeleteProc,
    .CloseProcedure  = nullptr,
    .PoolTag         = PS_TAG_THREAD,
    .ValidAccessMask = 0xFFFFFFFF,
};

static void s_VmDeleteProc(void *object)
{
    Ds_Vm *proc = object;
    Ob_DestroyHandleTable(&proc->HandleTable);
    if (proc->Token)
        Acl_DereferenceToken(proc->Token);
}

static void s_ThreadDeleteProc(void *object)
{
    Ds_Thread *thread = object;
    if (thread->StackBase && !(thread->Flags & PS_THREAD_IDLE))
        Ex_Free((void *)thread->StackBase);
}

void Ds_SystemInit(void)
{
    s_VmType     = Ob_CreateType(&s_VmInfo);
    s_ThreadType = Ob_CreateType(&s_ThreadInfo);

    Arch_AtomicStore32(&s_NextVmId, 1);
    Arch_AtomicStore32(&s_NextThreadId, 1);

    Ds_CreateVm(nullptr, &s_SystemVm);
    s_SystemVm->VmId = 0;

    Ob_CreateDirectory("\\Supervisor", nullptr);
    Ob_InsertObjectByPath("\\Supervisor\\System", s_SystemVm);
}

i32 Ds_CreateVm(Acl_Token *token, Ds_Vm **outVm)
{
    Ds_Vm *proc = nullptr;
    i32    st   = Ob_CreateObject(s_VmType, 0, (void **)&proc);
    if (st != kObSuccess)
        return st;

    Ds_DispatcherHeaderInit(&proc->Header, kDsObjectVm, 0);
    Dsa_ListInit(&proc->ThreadListHead);
    Core_SpinlockInit(&proc->ThreadListLock);
    proc->ThreadCount  = 0;
    proc->AddressSpace = nullptr;
    Ob_InitHandleTable(&proc->HandleTable);

    if (token)
    {
        Acl_ReferenceToken(token);
        proc->Token = token;
    }
    else
        proc->Token = nullptr;

    proc->VmId     = Arch_AtomicAdd32(&s_NextVmId, 1);
    proc->Flags    = 0;
    proc->ExitCode = 0;

    *outVm = proc;
    return kObSuccess;
}

Ds_Vm *Ds_GetSystemVm(void)
{
    return s_SystemVm;
}

Ob_Type *Ds_GetVmType(void)
{
    return s_VmType;
}

Ob_Type *Ds_GetThreadType(void)
{
    return s_ThreadType;
}

i32 Ds_CreateThread(Ds_Vm *process, Arch_ThreadEntry entry, void *param, u32 priority, Ds_Thread **outThread)
{
    Ds_Thread *th = nullptr;
    i32        st = Ob_CreateObject(s_ThreadType, 0, (void **)&th);
    if (st != kObSuccess)
        return st;

    Ds_DispatcherHeaderInit(&th->Header, kDsObjectThread, 0);
    Dsa_ListInit(&th->SchedLink);
    Dsa_ListInit(&th->VmThreadLink);

    th->StackBase = (uptr)Ex_Allocate(PS_THREAD_STACK_SIZE, PS_TAG_STACK);
    if (!th->StackBase)
    {
        Ob_DestroyObject(th);
        return kObInsufficientResources;
    }
    th->StackSize = PS_THREAD_STACK_SIZE;

    const uptr stackTop = th->StackBase + PS_THREAD_STACK_SIZE;

    th->EntryPoint     = entry;
    th->EntryParameter = param;

    Arch_ContextInit(&th->Context, stackTop, Ds_ThreadStartup, th);

    th->Vm              = process;
    th->State           = kDsThreadInitialized;
    th->BasePriority    = priority;
    th->CurrentPriority = priority;
    th->QuantumReset    = DS_QUANTUM_DEFAULT;
    th->Quantum         = th->QuantumReset;
    th->Flags           = 0;
    th->ExitCode        = 0;
    th->WaitTimerActive = false;
    th->ThreadId        = Arch_AtomicAdd32(&s_NextThreadId, 1);
    th->Vmor            = 0;
    th->IdealVmor       = 0;

    Arch_IrqFlags irq = Core_SpinlockAcquireIrq(&process->ThreadListLock);
    Dsa_ListInsertTail(&process->ThreadListHead, &th->VmThreadLink);
    process->ThreadCount++;
    Core_SpinlockReleaseIrq(&process->ThreadListLock, irq);

    *outThread = th;
    return kObSuccess;
}

void Ds_ReadyThread(Ds_Thread *thread)
{
    Irql_t old = Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    thread->State = kDsThreadReady;
    Ds_InsertReady(thread);
    Ds_CheckPreemption(&Arch_GetCurrentSpcb()->Scheduler);

    Core_SpinlockReleaseIrql(&g_DispatcherLock, old);
}

void Ds_ThreadExit(u32 exitCode)
{
    Core_SpinlockAcquireIrql(&g_DispatcherLock, IRQL_DISPATCH);

    Ds_Thread       *self = Ds_GetCurrentThread();
    Ds_SchedulerCpu *cpu  = &Arch_GetCurrentSpcb()->Scheduler;

    self->ExitCode = exitCode;
    self->State    = kDsThreadTerminated;

    self->Header.SignalState = 1;
    Ds_SignalObject(&self->Header);

    Ds_Vm        *proc = self->Vm;
    Arch_IrqFlags irq  = Core_SpinlockAcquireIrq(&proc->ThreadListLock);
    Dsa_ListRemoveEntry(&self->VmThreadLink);
    u32 remaining = --proc->ThreadCount;
    Core_SpinlockReleaseIrq(&proc->ThreadListLock, irq);

    if (remaining == 0)
    {
        proc->ExitCode           = exitCode;
        proc->Header.SignalState = 1;
        Ds_SignalObject(&proc->Header);
    }

    Ob_DereferenceObject(self);

    Ds_Thread *next = Ds_SelectNextThread(cpu);
    if (next == nullptr)
        next = cpu->IdleThread;

    next->State        = kDsThreadRunning;
    cpu->CurrentThread = next;
    next->Vmor         = cpu->ProcessorNumber;

    Arch_ContextSwitch(&self->Context, next->Context);

    unreachable();
}

Ds_Thread *Ds_GetCurrentThread(void)
{
    return Arch_GetCurrentSpcb()->Scheduler.CurrentThread;
}
