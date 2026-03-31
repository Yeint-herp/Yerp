#define DBG_MODULE "ObDir"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <executive/Object.h>
#include <executive/Pool.h>

#define EX_TAG_OBDE EX_TAG('O', 'b', 'D', 'e')

static u32 s_HashName(const char *name)
{
    u32 hash = 2166136261u;

    for (const char *p = name; *p; p++)
    {
        hash ^= *p;
        hash *= 16777619u;
    }

    return hash;
}

static bool s_NextComponent(const char **cursor, char *dst, usize bufSize)
{
    const char *p = *cursor;

    while (*p == OB_PATH_SEPARATOR)
        p++;

    if (*p == '\0')
        return false;

    usize written = 0;

    while (*p && *p != OB_PATH_SEPARATOR && written < bufSize - 1)
        dst[written++] = *p++;

    dst[written] = '\0';
    *cursor      = p;

    return written > 0;
}

static bool s_IsLastComponent(const char *cursor)
{
    while (*cursor == OB_PATH_SEPARATOR)
        cursor++;

    return *cursor == '\0';
}

static i32 s_WalkToParent(const char *path, Ob_Directory **outParent, const char **outLeaf)
{
    if (!path || path[0] != OB_PATH_SEPARATOR)
        return OB_INVALID_PARAMETER;

    Ob_Directory *current = Ob_GetRootDirectory();
    const char   *cursor  = path;
    char          component[OB_MAX_COMPONENT];
    bool          currentIsRoot = true;

    Ob_ReferenceObject(current);

    while (s_NextComponent(&cursor, component, sizeof component))
    {
        if (s_IsLastComponent(cursor))
        {
            const char *leafStart = cursor;
            while (leafStart > path && *(leafStart - 1) != OB_PATH_SEPARATOR)
                leafStart--;

            *outParent = current;
            *outLeaf   = leafStart;
            return OB_SUCCESS;
        }

        void     *child  = nullptr;
        const i32 status = Ob_LookupObject(current, component, Ob_GetDirectoryType(), &child);

        if (!currentIsRoot)
            Ob_DereferenceObject(current);

        if (status != OB_SUCCESS)
            return status;

        current       = child;
        currentIsRoot = false;
    }

    return OB_INVALID_PARAMETER;
}

i32 Ob_InsertObject(Ob_Directory *dir, void *object, const char *name)
{
    if (!dir || !object || !name || !name[0])
        return OB_INVALID_PARAMETER;

    u32 bucket = s_HashName(name) % OB_DIRECTORY_BUCKETS;

    Core_SpinlockAcquire(&dir->Lock);

    for (Ob_DirectoryEntry *e = dir->Buckets[bucket]; e; e = e->Next)
    {
        if (Core_CompareString(e->Name, name) == 0)
        {
            Core_SpinlockRelease(&dir->Lock);
            Log(TRACE, "name collision inserting '%s'", name);

            return OB_NAME_COLLISION;
        }
    }

    Ob_DirectoryEntry *entry = Ex_Allocate(sizeof *entry, EX_TAG_OBDE);

    if (!entry)
    {
        Core_SpinlockRelease(&dir->Lock);
        return OB_INSUFFICIENT_RESOURCES;
    }

    entry->Name   = Core_DuplicateString(name, EX_TAG_OBDE);
    entry->Object = object;
    entry->Next   = dir->Buckets[bucket];

    dir->Buckets[bucket] = entry;

    Ob_Header *hdr = Ob_HeaderFromBody(object);
    hdr->NameEntry = entry;

    Ob_ReferenceObject(object);

    Core_SpinlockRelease(&dir->Lock);

    Log(TRACE, "inserted '%s' into directory", name);
    return OB_SUCCESS;
}

i32 Ob_LookupObject(Ob_Directory *dir, const char *name, Ob_Type *expectedType, void **outObject)
{
    if (!dir || !name || !outObject)
        return OB_INVALID_PARAMETER;

    u32 bucket = s_HashName(name) % OB_DIRECTORY_BUCKETS;

    Core_SpinlockAcquire(&dir->Lock);

    for (Ob_DirectoryEntry *e = dir->Buckets[bucket]; e; e = e->Next)
    {
        if (Core_CompareString(e->Name, name) != 0)
            continue;

        if (expectedType)
        {
            Ob_Header *hdr = Ob_HeaderFromBody(e->Object);
            if (hdr->Type != expectedType)
            {
                Core_SpinlockRelease(&dir->Lock);
                return OB_TYPE_MISMATCH;
            }
        }

        Ob_ReferenceObject(e->Object);
        *outObject = e->Object;

        Core_SpinlockRelease(&dir->Lock);
        return OB_SUCCESS;
    }

    Core_SpinlockRelease(&dir->Lock);
    return OB_NOT_FOUND;
}

i32 Ob_RemoveObject(Ob_Directory *dir, const char *name)
{
    if (!dir || !name)
        return OB_INVALID_PARAMETER;

    u32 bucket = s_HashName(name) % OB_DIRECTORY_BUCKETS;

    Core_SpinlockAcquire(&dir->Lock);

    Ob_DirectoryEntry **prev = &dir->Buckets[bucket];

    for (Ob_DirectoryEntry *e = *prev; e; prev = &e->Next, e = e->Next)
    {
        if (Core_CompareString(e->Name, name) != 0)
            continue;

        *prev = e->Next;

        Ob_Header *hdr = Ob_HeaderFromBody(e->Object);
        hdr->NameEntry = nullptr;

        void *object = e->Object;
        Ex_Free(e->Name);
        Ex_Free(e);

        Core_SpinlockRelease(&dir->Lock);

        Ob_DereferenceObject(object);

        Log(TRACE, "removed '%s' from directory", name);
        return OB_SUCCESS;
    }

    Core_SpinlockRelease(&dir->Lock);
    return OB_NOT_FOUND;
}

i32 Ob_InsertObjectByPath(const char *path, void *object)
{
    Ob_Directory *parent = nullptr;
    const char   *leaf   = nullptr;

    i32 status = s_WalkToParent(path, &parent, &leaf);
    if (status != OB_SUCCESS)
        return status;

    status = Ob_InsertObject(parent, object, leaf);
    Ob_DereferenceObject(parent);

    return status;
}

i32 Ob_LookupObjectByPath(const char *path, Ob_Type *expectedType, void **outObject)
{
    Ob_Directory *parent = nullptr;
    const char   *leaf   = nullptr;

    i32 status = s_WalkToParent(path, &parent, &leaf);
    if (status != OB_SUCCESS)
        return status;

    status = Ob_LookupObject(parent, leaf, expectedType, outObject);
    Ob_DereferenceObject(parent);

    return status;
}

i32 Ob_RemoveObjectByPath(const char *path)
{
    Ob_Directory *parent = nullptr;
    const char   *leaf   = nullptr;

    i32 status = s_WalkToParent(path, &parent, &leaf);
    if (status != OB_SUCCESS)
        return status;

    status = Ob_RemoveObject(parent, leaf);
    Ob_DereferenceObject(parent);

    return status;
}

i32 Ob_CreateDirectory(const char *path, Ob_Directory **outDir)
{
    Ob_Directory *dir = nullptr;

    i32 status = Ob_CreateObject(Ob_GetDirectoryType(), 0, (void **)&dir);
    if (status != OB_SUCCESS)
        return status;

    Core_ZeroMemory(dir, sizeof *dir);

    status = Ob_InsertObjectByPath(path, dir);
    if (status != OB_SUCCESS)
    {
        Ob_DereferenceObject(dir);
        return status;
    }

    if (outDir)
        *outDir = dir;
    else
        Ob_DereferenceObject(dir);

    return OB_SUCCESS;
}

i32 Ob_OpenObjectByPath(const char *path, Ob_Type *expectedType, u32 desiredAccess, Acl_Token *token,
                        Ob_HandleTable *table, Ob_Handle *outHandle)
{
    if (!path || !token || !table || !outHandle)
        return OB_INVALID_PARAMETER;

    void *object = nullptr;
    i32   status = Ob_LookupObjectByPath(path, expectedType, &object);

    if (status != OB_SUCCESS)
        return status;

    status = Ob_InsertHandle(table, object, desiredAccess, token, outHandle);
    Ob_DereferenceObject(object);

    return status;
}
