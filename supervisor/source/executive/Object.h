#ifndef SUPERVISOR_EXECUTIVE_OBJECT_H
#define SUPERVISOR_EXECUTIVE_OBJECT_H

#include <arch/Atomic.h>
#include <core/Spinlock.h>
#include <executive/Acl.h>

enum
{
    kObSuccess,
    kObInvalidParameter,
    kObInsufficientResources,
    kObNotFound,
    kObNameCollision,
    kObTypeMismatch,
    kObAccessDenied,
};

typedef void (*Ob_DeleteProcedure)(void *Object);
typedef void (*Ob_CloseProcedure)(void *Object, usize HandleCount);

typedef struct Ob_TypeInfo
{
    const char        *Name;
    usize              ObjectBodySize;
    Ob_DeleteProcedure DeleteProcedure;
    Ob_CloseProcedure  CloseProcedure;
    u32                PoolTag;
    u32                ValidAccessMask;
} Ob_TypeInfo;

typedef struct Ob_Type
{
    Ob_TypeInfo Info;
    u32         Index;
    u32         TotalObjects;

    struct Ob_Type *Next;
} Ob_Type;

typedef struct Ob_Header
{
    Ob_Type      *Type;
    Arch_Atomic32 ReferenceCount;
    Arch_Atomic32 HandleCount;

    Core_Spinlock Lock;

    struct Ob_DirectoryEntry *NameEntry;

    Acl_Sid  Owner;
    Acl_Acl *Acl;

    u32 PoolTag;
    u32 Flags;
} Ob_Header;

#define Ob_HeaderFromBody(body) ((Ob_Header *)((uptr)(body) - sizeof(Ob_Header)))
#define Ob_BodyFromHeader(hdr)  ((void *)((uptr)(hdr) + sizeof(Ob_Header)))

void Ob_SetObjectSecurity(void *object, Acl_Sid owner, Acl_Acl *acl);

typedef struct Ob_DirectoryEntry
{
    char *Name;
    void *Object;

    struct Ob_DirectoryEntry *Next;
} Ob_DirectoryEntry;

#define OB_DIRECTORY_BUCKETS 37

typedef struct Ob_Directory
{
    Ob_DirectoryEntry *Buckets[OB_DIRECTORY_BUCKETS];

    Core_Spinlock Lock;
} Ob_Directory;

#define OB_HANDLE_TABLE_INITIAL_SIZE 64

typedef struct Ob_HandleEntry
{
    void *Object;

    union
    {
        u32 Access;
        u32 NextFree;
    } u1;
} Ob_HandleEntry;

typedef struct Ob_HandleTable
{
    Ob_HandleEntry *Entries;
    u32             Capacity;
    u32             FirstFree;

    Core_Spinlock Lock;
} Ob_HandleTable;

#define OB_PATH_SEPARATOR '\\'
#define OB_MAX_COMPONENT  128

typedef uptr Ob_Handle;
#define OB_HANDLE_NULL ((Ob_Handle) - 1)

Ob_Type *Ob_CreateType(const Ob_TypeInfo *info);

i32  Ob_CreateObject(Ob_Type *type, u32 flags, void **outObject);
void Ob_DestroyObject(void *object);

void Ob_ReferenceObject(void *object);
bool Ob_DereferenceObject(void *object);

void Ob_IncrementHandleCount(void *object);
void Ob_DecrementHandleCount(void *object);

void Ob_InitHandleTable(Ob_HandleTable *table);
void Ob_DestroyHandleTable(Ob_HandleTable *table);

i32 Ob_InsertHandleRaw(Ob_HandleTable *table, void *object, u32 access, Ob_Handle *outHandle);
i32 Ob_InsertHandle(Ob_HandleTable *table, void *object, u32 desiredAccess, Acl_Token *token, Ob_Handle *outHandle);

i32  Ob_ReferenceByHandle(Ob_HandleTable *table, Ob_Handle handle, Ob_Type *expectedType, void **outObject);
void Ob_CloseHandle(Ob_HandleTable *table, Ob_Handle handle);
u32  Ob_GetHandleAccess(Ob_HandleTable *table, Ob_Handle handle);

i32 Ob_CheckHandleAccess(Ob_HandleTable *table, Ob_Handle handle, u32 requiredAccess);

Ob_Directory *Ob_GetRootDirectory(void);
Ob_Type      *Ob_GetDirectoryType(void);

i32 Ob_InsertObject(Ob_Directory *dir, void *object, const char *name);
i32 Ob_LookupObject(Ob_Directory *dir, const char *name, Ob_Type *expectedType, void **outObject);
i32 Ob_RemoveObject(Ob_Directory *dir, const char *name);

i32 Ob_InsertObjectByPath(const char *path, void *object);
i32 Ob_LookupObjectByPath(const char *path, Ob_Type *expectedType, void **outObject);
i32 Ob_RemoveObjectByPath(const char *path);

i32 Ob_CreateDirectory(const char *path, Ob_Directory **outDir);

i32 Ob_OpenObjectByPath(const char *path, Ob_Type *expectedType, u32 desiredAccess, Acl_Token *token,
                        Ob_HandleTable *table, Ob_Handle *outHandle);

Ob_Type   *Ob_GetObjectType(void *object);
Ob_Header *Ob_GetHeader(void *object);
i32        Ob_GetReferenceCount(void *object);
i32        Ob_GetHandleCount(void *object);

void Ob_Init(void);

#endif /* SUPERVISOR_EXECUTIVE_OBJECT_H */
