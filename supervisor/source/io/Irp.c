#define DBG_MODULE "Irp"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <executive/Pool.h>
#include <io/Device.h>
#include <io/Irp.h>

Io_Irp *Io_AllocateIrp(u32 stackSize)
{
    if (stackSize == 0)
        return nullptr;

    usize   totalSize = sizeof(Io_Irp) + stackSize * sizeof(Io_StackLocation);
    Io_Irp *irp       = Ex_Allocate(totalSize, IO_TAG_IRP);
    if (!irp)
        return nullptr;

    Core_ZeroMemory(irp, totalSize);

    irp->StackCount      = stackSize;
    irp->CurrentLocation = stackSize;

    Ds_EventInit(&irp->CompletionEvent, kDsObjectNotificationEvent, false);

    return irp;
}

void Io_FreeIrp(Io_Irp *irp)
{
    if (irp)
        Ex_Free(irp);
}

Io_StackLocation *Io_GetCurrentStackLocation(Io_Irp *irp)
{
    return &irp->Stack[irp->CurrentLocation];
}

Io_StackLocation *Io_GetNextStackLocation(Io_Irp *irp)
{
    return &irp->Stack[irp->CurrentLocation - 1];
}

void Io_SetCompletionRoutine(Io_Irp *irp, Io_CompletionRoutine routine, void *context, u8 controlFlags)
{
    Io_StackLocation *next  = Io_GetNextStackLocation(irp);
    next->CompletionRoutine = routine;
    next->CompletionContext = context;
    next->Control           = controlFlags;
}

void Io_SkipCurrentStackLocation(Io_Irp *irp)
{
    irp->CurrentLocation++;
}

void Io_CopyCurrentToNext(Io_Irp *irp)
{
    Io_StackLocation *cur  = Io_GetCurrentStackLocation(irp);
    Io_StackLocation *next = Io_GetNextStackLocation(irp);

    Core_CopyMemory(&next->Parameters, &cur->Parameters, sizeof cur->Parameters);
    next->MajorFunction = cur->MajorFunction;
    next->MinorFunction = cur->MinorFunction;
    next->Flags         = cur->Flags;
    next->FileObject    = cur->FileObject;
}

void Io_CompleteRequest(Io_Irp *irp, i32 status, usize information)
{
    irp->Status      = status;
    irp->Information = information;

    for (u32 i = irp->CurrentLocation; i < irp->StackCount; i++)
    {
        Io_StackLocation *sl = &irp->Stack[i];

        if (!sl->CompletionRoutine)
            continue;

        bool success   = (status >= 0);
        bool cancelled = irp->Cancel;

        bool invoke = false;
        if (success && (sl->Control & IO_SL_INVOKE_ON_SUCCESS))
            invoke = true;
        if (!success && !cancelled && (sl->Control & IO_SL_INVOKE_ON_ERROR))
            invoke = true;
        if (cancelled && (sl->Control & IO_SL_INVOKE_ON_CANCEL))
            invoke = true;

        if (invoke)
            sl->CompletionRoutine(sl->Device, irp, sl->CompletionContext);
    }

    if ((irp->Flags & IRP_FLAG_BUFFERED) && irp->SystemBuffer)
    {
        if (irp->Stack[irp->StackCount - 1].MajorFunction == IRP_MJ_READ && information > 0 && irp->UserBuffer)
            Core_CopyMemory(irp->UserBuffer, irp->SystemBuffer, information);

        Ex_Free(irp->SystemBuffer);
        irp->SystemBuffer = nullptr;
    }

    Ds_EventSet(&irp->CompletionEvent);
}

void Io_MarkIrpPending(Io_Irp *irp)
{
    Io_StackLocation *sl = Io_GetCurrentStackLocation(irp);
    sl->Control |= (1u << 7);
}

void Io_CancelIrp(Io_Irp *irp)
{
    irp->Cancel = true;

    Io_CancelRoutine cancelRoutine = irp->CancelRoutine;
    if (cancelRoutine)
    {
        irp->CancelRoutine   = nullptr;
        Io_StackLocation *sl = Io_GetCurrentStackLocation(irp);
        cancelRoutine(sl->Device, irp);
    }
}
