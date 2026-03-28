#define DBG_MODULE "ObCreate"

#include <arch/Atomic.h>
#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Object.h>
#include <mm/Pool.h>

#define OB_MAX_TYPES 64

#define EX_TAG_OBTYPE EX_TAG('O', 'b', 'T', 'y')
#define EX_TAG_OBDIR  EX_TAG('O', 'b', 'D', 'r')

static Ob_Type  s_TypeTable[OB_MAX_TYPES] = {};
static u32      s_TypeCount               = {};
static Ob_Type *s_TypeFreeList            = nullptr;

static Ob_Type      *s_DirectoryType = nullptr;
static Ob_Directory *s_RootDirectory = nullptr;

Ob_Type *Ob_CreateType(const Ob_TypeInfo *info)
{
    if (s_TypeCount >= OB_MAX_TYPES)
    {
        Log(ERROR, "type table exhausted registering '%s'", info->Name);
        return nullptr;
    }

    Ob_Type *type      = &s_TypeTable[s_TypeCount];
    type->Info         = *info;
    type->Index        = s_TypeCount;
    type->TotalObjects = 0;
    type->Next         = s_TypeFreeList;
    s_TypeFreeList     = type;
    s_TypeCount++;

    Log(TRACE, "registered type '%s' (index %u, body %zu bytes)", info->Name, type->Index, info->ObjectBodySize);

    return type;
}

i32 Ob_CreateObject(Ob_Type *type, u32 flags, void **outObject)
{
    if (!type || !outObject)
        return OB_INVALID_PARAMETER;

    const usize totalSize = sizeof(Ob_Header) + type->Info.ObjectBodySize;

    Ob_Header *hdr = Ex_Allocate(totalSize, type->Info.PoolTag);
    if (!hdr)
        return OB_INSUFFICIENT_RESOURCES;

    Core_ZeroMemory(hdr, totalSize);

    hdr->Type           = type;
    hdr->ReferenceCount = 1;
    hdr->HandleCount    = 0;
    hdr->NameEntry      = nullptr;
    hdr->Owner          = ACL_SID_SUPERVISOR;
    hdr->Acl            = nullptr;
    hdr->PoolTag        = type->Info.PoolTag;
    hdr->Flags          = flags;

    Arch_AtomicAdd32(&type->TotalObjects, 1);

    void *body = Ob_BodyFromHeader(hdr);
    *outObject = body;

    return OB_SUCCESS;
}

void Ob_DestroyObject(void *object)
{
    Ob_Header *hdr  = Ob_HeaderFromBody(object);
    Ob_Type   *type = hdr->Type;

    if (hdr->NameEntry)
    {
        Ex_Free(hdr->NameEntry->Name);
        Ex_Free(hdr->NameEntry);
        hdr->NameEntry = nullptr;
    }

    if (hdr->Acl)
    {
        Acl_DestroyAcl(hdr->Acl);
        hdr->Acl = nullptr;
    }

    if (type->Info.DeleteProcedure)
        type->Info.DeleteProcedure(object);

    Arch_AtomicSub32(&type->TotalObjects, 1);

    Log(TRACE, "destroyed object of type '%s'", type->Info.Name);

    Ex_Free(hdr);
}

Ob_Type *Ob_GetObjectType(void *object)
{
    return Ob_HeaderFromBody(object)->Type;
}

Ob_Header *Ob_GetHeader(void *object)
{
    return Ob_HeaderFromBody(object);
}

Ob_Type *Ob_GetDirectoryType(void)
{
    return s_DirectoryType;
}

Ob_Directory *Ob_GetRootDirectory(void)
{
    return s_RootDirectory;
}

void Ob_SetObjectSecurity(void *object, Acl_Sid owner, Acl_Acl *acl)
{
    Ob_Header *hdr = Ob_HeaderFromBody(object);

    Core_SpinlockAcquire(&hdr->Lock);

    if (hdr->Acl)
        Acl_DestroyAcl(hdr->Acl);

    hdr->Owner = owner;
    hdr->Acl   = acl;

    Core_SpinlockRelease(&hdr->Lock);
}

void Ob_Init(void)
{
    Ob_TypeInfo dirInfo = {
        .Name            = "Directory",
        .ObjectBodySize  = sizeof(Ob_Directory),
        .DeleteProcedure = nullptr,
        .CloseProcedure  = nullptr,
        .PoolTag         = EX_TAG_OBDIR,
        .ValidAccessMask = 0,
    };
    s_DirectoryType = Ob_CreateType(&dirInfo);

    Ob_CreateObject(s_DirectoryType, 0, (void **)&s_RootDirectory);
    Core_SpinlockInit(&s_RootDirectory->Lock);
    for (u32 i = 0; i < OB_DIRECTORY_BUCKETS; i++)
        s_RootDirectory->Buckets[i] = nullptr;

    Log(INFO, "object manager initialized");
}
