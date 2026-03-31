#define DBG_MODULE "ObHandle"

#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Object.h>
#include <executive/Pool.h>

#define EX_TAG_OBHT EX_TAG('O', 'b', 'H', 't')

#define HANDLE_FREE_END ((u32) - 1)

void Ob_InitHandleTable(Ob_HandleTable *table)
{
    table->Capacity = OB_HANDLE_TABLE_INITIAL_SIZE;
    table->Entries  = Ex_Allocate(sizeof *table->Entries * table->Capacity, EX_TAG_OBHT);

    if (!table->Entries)
        Panic("failed to allocate initial handle table");

    for (u32 i = 0; i < table->Capacity; i++)
    {
        table->Entries[i].Object      = nullptr;
        table->Entries[i].u1.NextFree = (i + 1 < table->Capacity) ? i + 1 : HANDLE_FREE_END;
    }

    table->FirstFree = 0;
    Core_SpinlockInit(&table->Lock);
}

void Ob_DestroyHandleTable(Ob_HandleTable *table)
{
    for (u32 i = 0; i < table->Capacity; i++)
    {
        if (table->Entries[i].Object)
        {
            Ob_DecrementHandleCount(table->Entries[i].Object);
            table->Entries[i].Object = nullptr;
        }
    }

    Ex_Free(table->Entries);
    table->Entries  = nullptr;
    table->Capacity = 0;
}

static i32 GrowHandleTable(Ob_HandleTable *table)
{
    const u32       newCap     = table->Capacity * 2;
    const usize     newSize    = sizeof(Ob_HandleEntry) * newCap;
    Ob_HandleEntry *newEntries = Ex_Allocate(newSize, EX_TAG_OBHT);

    if (!newEntries)
        return OB_INSUFFICIENT_RESOURCES;

    for (u32 i = 0; i < table->Capacity; i++)
        newEntries[i] = table->Entries[i];

    for (u32 i = table->Capacity; i < newCap; i++)
    {
        newEntries[i].Object      = nullptr;
        newEntries[i].u1.NextFree = (i + 1 < newCap) ? i + 1 : HANDLE_FREE_END;
    }

    table->FirstFree = table->Capacity;

    Ob_HandleEntry *old = table->Entries;
    table->Entries      = newEntries;
    table->Capacity     = newCap;

    Ex_Free(old);

    Log(TRACE, "handle table grown to %u entries", newCap);
    return OB_SUCCESS;
}

i32 Ob_InsertHandleRaw(Ob_HandleTable *table, void *object, u32 access, Ob_Handle *outHandle)
{
    if (!table || !object || !outHandle)
        return OB_INVALID_PARAMETER;

    Core_SpinlockAcquire(&table->Lock);

    if (table->FirstFree == HANDLE_FREE_END)
    {
        const i32 status = GrowHandleTable(table);
        if (status != OB_SUCCESS)
        {
            Core_SpinlockRelease(&table->Lock);
            return status;
        }
    }

    const u32       index = table->FirstFree;
    Ob_HandleEntry *entry = &table->Entries[index];

    table->FirstFree = entry->u1.NextFree;

    entry->Object    = object;
    entry->u1.Access = access;

    Core_SpinlockRelease(&table->Lock);

    Ob_IncrementHandleCount(object);

    *outHandle = (Ob_Handle)index;
    return OB_SUCCESS;
}

i32 Ob_InsertHandle(Ob_HandleTable *table, void *object, u32 desiredAccess, Acl_Token *token, Ob_Handle *outHandle)
{
    if (!table || !object || !outHandle || !token)
        return OB_INVALID_PARAMETER;

    Ob_Header *hdr = Ob_HeaderFromBody(object);

    if (desiredAccess & ~hdr->Type->Info.ValidAccessMask)
        return OB_INVALID_PARAMETER;

    u32 granted = Acl_ComputeAccess(hdr->Acl, token, desiredAccess);

    if ((granted & desiredAccess) != desiredAccess)
        return OB_ACCESS_DENIED;

    return Ob_InsertHandleRaw(table, object, granted, outHandle);
}

i32 Ob_ReferenceByHandle(Ob_HandleTable *table, Ob_Handle handle, Ob_Type *expectedType, void **outObject)
{
    if (!table || !outObject)
        return OB_INVALID_PARAMETER;

    Core_SpinlockAcquire(&table->Lock);

    const u32 index = handle;

    if (index >= table->Capacity || !table->Entries[index].Object)
    {
        Core_SpinlockRelease(&table->Lock);
        return OB_INVALID_PARAMETER;
    }

    void *object = table->Entries[index].Object;

    if (expectedType)
    {
        Ob_Header *hdr = Ob_HeaderFromBody(object);
        if (hdr->Type != expectedType)
        {
            Core_SpinlockRelease(&table->Lock);
            return OB_TYPE_MISMATCH;
        }
    }

    Ob_ReferenceObject(object);

    *outObject = object;
    Core_SpinlockRelease(&table->Lock);

    return OB_SUCCESS;
}

void Ob_CloseHandle(Ob_HandleTable *table, Ob_Handle handle)
{
    Core_SpinlockAcquire(&table->Lock);

    const u32 index = handle;

    if (index >= table->Capacity || !table->Entries[index].Object)
    {
        Core_SpinlockRelease(&table->Lock);
        Panic("Ob_CloseHandle: invalid handle %zu", handle);
    }

    void *object = table->Entries[index].Object;

    table->Entries[index].Object      = nullptr;
    table->Entries[index].u1.NextFree = table->FirstFree;
    table->FirstFree                  = index;

    Core_SpinlockRelease(&table->Lock);

    Ob_DecrementHandleCount(object);
}

u32 Ob_GetHandleAccess(Ob_HandleTable *table, Ob_Handle handle)
{
    Core_SpinlockAcquire(&table->Lock);

    const u32 index = handle;

    if (index >= table->Capacity || !table->Entries[index].Object)
    {
        Core_SpinlockRelease(&table->Lock);
        return 0;
    }

    const u32 access = table->Entries[index].u1.Access;
    Core_SpinlockRelease(&table->Lock);

    return access;
}

i32 Ob_CheckHandleAccess(Ob_HandleTable *table, Ob_Handle handle, u32 requiredAccess)
{
    const u32 granted = Ob_GetHandleAccess(table, handle);

    if ((granted & requiredAccess) != requiredAccess)
        return OB_ACCESS_DENIED;

    return OB_SUCCESS;
}
