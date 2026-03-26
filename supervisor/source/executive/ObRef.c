#define DBG_MODULE "ObRef"

#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Object.h>

void Ob_ReferenceObject(void *object)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);
    i32        old = Arch_AtomicAdd32(&hdr->ReferenceCount, 1);

    if (old <= 0)
        Panic("Ob_ReferenceObject: referencing object with count %d (type '%s')", old, hdr->Type->Info.Name);
}

bool Ob_DereferenceObject(void *object)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);

    i32 old = Arch_AtomicSub32(&hdr->ReferenceCount, 1);

    if (old < 1)
        Panic("Ob_DereferenceObject: underflow on object with count %d (type '%s')", old, hdr->Type->Info.Name);

    if (old == 1)
    {
        Ob_DestroyObject(object);
        return true;
    }

    return false;
}
void Ob_IncrementHandleCount(void *object)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);

    Arch_AtomicAdd32(&hdr->HandleCount, 1);

    Ob_ReferenceObject(object);
}

void Ob_DecrementHandleCount(void *object)
{
    Ob_Header *hdr  = Ob_HeaderFromBody(object);
    Ob_Type   *type = hdr->Type;

    const i32 old = Arch_AtomicSub32(&hdr->HandleCount, 1);

    if (old < 1)
        Panic("Ob_DecrementHandleCount: underflow (type '%s')", type->Info.Name);

    if (type->Info.CloseProcedure)
        type->Info.CloseProcedure(object, old - 1);

    Ob_DereferenceObject(object);
}

i32 Ob_GetReferenceCount(void *object)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);
    return Arch_AtomicLoad32(&hdr->ReferenceCount);
}

i32 Ob_GetHandleCount(void *object)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);
    return Arch_AtomicLoad32(&hdr->HandleCount);
}
