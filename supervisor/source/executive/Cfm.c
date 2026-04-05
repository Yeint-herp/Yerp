#include <core/Memory.h>
#include <executive/Cfm.h>

#define CF_PATH_SEPARATOR '\\'

static Ob_Type *s_KeyType;
static Cf_Key  *s_RootKey;

static i32 s_VolatileFlush(Cf_Mount *mount, Cf_Key *key)
{
    (void)mount;
    (void)key;
    return kCfSuccess;
}

static i32 s_VolatileDelete(Cf_Mount *mount, Cf_Key *key)
{
    (void)mount;
    (void)key;
    return kCfSuccess;
}

static const Cf_MountOps s_VolatileOps = {
    .FlushKey  = s_VolatileFlush,
    .DeleteKey = s_VolatileDelete,
    .Unload    = nullptr,
};

static Cf_Mount s_VolatileMount = {
    .Ops     = &s_VolatileOps,
    .Context = nullptr,
    .Flags   = CF_MOUNT_VOLATILE,
};

static i32 s_SubKeyCmp(Dsa_AvlNode *a, Dsa_AvlNode *b)
{
    Cf_Key *ka = Dsa_AvlEntry(a, Cf_Key, SiblingNode);
    Cf_Key *kb = Dsa_AvlEntry(b, Cf_Key, SiblingNode);

    return Core_CompareString(ka->Name, kb->Name);
}

static i32 s_SubKeyFind(void *name, Dsa_AvlNode *node)
{
    Cf_Key *k = Dsa_AvlEntry(node, Cf_Key, SiblingNode);

    return Core_CompareString(name, k->Name);
}

static i32 s_ValueCmp(Dsa_AvlNode *a, Dsa_AvlNode *b)
{
    Cf_Value *va = Dsa_AvlEntry(a, Cf_Value, TreeNode);
    Cf_Value *vb = Dsa_AvlEntry(b, Cf_Value, TreeNode);

    return Core_CompareString(va->Name, vb->Name);
}

static i32 s_ValueFind(void *name, Dsa_AvlNode *node)
{
    Cf_Value *v = Dsa_AvlEntry(node, Cf_Value, TreeNode);

    return Core_CompareString(name, v->Name);
}

static void s_FreeValue(Cf_Value *val)
{
    if (val->Name)
        Ex_Free(val->Name);

    if (val->Data)
        Ex_Free(val->Data);

    Ex_Free(val);
}

static void s_FreeAllValues(Dsa_AvlTree *tree)
{
    Dsa_AvlNode *node = Dsa_AvlFirst(tree);
    while (node)
    {
        Dsa_AvlNode *next = Dsa_AvlNext(node);
        s_FreeValue(Dsa_AvlEntry(node, Cf_Value, TreeNode));
        node = next;
    }

    tree->Root  = nullptr;
    tree->Count = 0;
}

static Cf_Value *s_FindValue(Cf_Key *key, const char *name)
{
    Dsa_AvlNode *node = Dsa_AvlFind(&key->Values, name, s_ValueFind);

    return node ? Dsa_AvlEntry(node, Cf_Value, TreeNode) : nullptr;
}

static Cf_Key *s_FindSubKey(Cf_Key *parent, const char *name)
{
    Dsa_AvlNode *node = Dsa_AvlFind(&parent->SubKeys, name, s_SubKeyFind);

    return node ? Dsa_AvlEntry(node, Cf_Key, SiblingNode) : nullptr;
}

static Cf_Key *s_WalkPath(Cf_Key *start, const char *path)
{
    if (!path || *path == '\0')
    {
        Ob_ReferenceObject(start);
        return start;
    }

    Cf_Key *current = start;
    Ob_ReferenceObject(current);

    while (*path)
    {
        while (*path == CF_PATH_SEPARATOR)
            path++;

        if (*path == '\0')
            break;

        const char *end = path;
        while (*end && *end != CF_PATH_SEPARATOR)
            end++;

        usize len = end - path;
        if (len == 0 || len >= OB_MAX_COMPONENT)
        {
            Ob_DereferenceObject(current);
            return nullptr;
        }

        char component[OB_MAX_COMPONENT];
        Core_CopyMemory(component, path, len);
        component[len] = '\0';

        Core_SpinlockAcquire(&current->Lock);
        Cf_Key *child = s_FindSubKey(current, component);
        if (child)
            Ob_ReferenceObject(child);

        Core_SpinlockRelease(&current->Lock);

        Ob_DereferenceObject(current);

        if (!child)
            return nullptr;

        current = child;
        path    = end;
    }

    return current;
}

static i32 s_ResolveKey(Ob_HandleTable *table, Ob_Handle handle, Cf_Key **outKey)
{
    if (handle == OB_HANDLE_NULL)
    {
        Ob_ReferenceObject(s_RootKey);
        *outKey = s_RootKey;
        return kCfSuccess;
    }

    i32 status = Ob_ReferenceByHandle(table, handle, s_KeyType, (void **)outKey);
    return (status == kObSuccess) ? kCfSuccess : kCfInvalidParameter;
}

static void s_KeyDelete(void *object)
{
    Cf_Key *key = object;

    if (key->Mount && key->Mount->Ops->DeleteKey)
        key->Mount->Ops->DeleteKey(key->Mount, key);

    s_FreeAllValues(&key->Values);

    if (key->Parent)
    {
        Core_SpinlockAcquire(&key->Parent->Lock);
        Dsa_AvlRemove(&key->Parent->SubKeys, key->Name, s_SubKeyFind);
        Core_SpinlockRelease(&key->Parent->Lock);
        Ob_DereferenceObject(key->Parent);
        key->Parent = nullptr;
    }

    if (key->Name)
        Ex_Free(key->Name);
}

static void s_KeyClose(void *object, usize handleCount)
{
    (void)object;
    (void)handleCount;
}

static i32 s_AllocateKey(Cf_Key *parent, const char *name, Cf_Mount *mount, Cf_Key **outKey)
{
    void *body;
    i32   status = Ob_CreateObject(s_KeyType, 0, &body);
    if (status != kObSuccess)
        return kCfInsufficientResources;

    Cf_Key *key = body;

    key->Name = Core_DuplicateString(name, EX_TAG_CFKEY);
    if (!key->Name)
    {
        Ob_DereferenceObject(key);
        return kCfInsufficientResources;
    }

    key->Parent = parent;
    if (parent)
        Ob_ReferenceObject(parent);

    Dsa_AvlTreeInit(&key->SubKeys);
    Dsa_AvlTreeInit(&key->Values);
    Core_SpinlockInit(&key->Lock);

    key->Mount = mount;
    key->Flags = 0;

    if (mount->Flags & CF_MOUNT_VOLATILE)
        key->Flags |= CF_KEY_FLAG_DIRTY;

    Acl_Sid  owner = parent ? Ob_GetHeader(parent)->Owner : ACL_SID_SUPERVISOR;
    Acl_Acl *acl   = Acl_CreateSimple(owner, CF_KEY_ALL_ACCESS, CF_KEY_QUERY_VALUE | CF_KEY_ENUMERATE);
    Ob_SetObjectSecurity(key, owner, acl);

    *outKey = key;
    return kCfSuccess;
}

i32 Cf_CreateKey(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *name, u32 desiredAccess,
                 Ob_Handle *outHandle)
{
    if (!table || !name || !outHandle)
        return kCfInvalidParameter;

    Cf_Key *parentKey;
    i32     status = s_ResolveKey(table, parent, &parentKey);
    if (status != kCfSuccess)
        return status;

    if (parent != OB_HANDLE_NULL)
    {
        i32 accessStatus = Ob_CheckHandleAccess(table, parent, CF_KEY_CREATE_SUBKEY);
        if (accessStatus != kObSuccess)
        {
            Ob_DereferenceObject(parentKey);
            return kCfAccessDenied;
        }
    }

    Core_SpinlockAcquire(&parentKey->Lock);
    if (s_FindSubKey(parentKey, name))
    {
        Core_SpinlockRelease(&parentKey->Lock);
        Ob_DereferenceObject(parentKey);
        return kCfNameCollision;
    }
    Core_SpinlockRelease(&parentKey->Lock);

    Cf_Key *newKey;
    status = s_AllocateKey(parentKey, name, parentKey->Mount, &newKey);
    if (status != kCfSuccess)
    {
        Ob_DereferenceObject(parentKey);
        return status;
    }

    Core_SpinlockAcquire(&parentKey->Lock);

    if (s_FindSubKey(parentKey, name))
    {
        Core_SpinlockRelease(&parentKey->Lock);
        Ob_DereferenceObject(parentKey);
        Ob_DereferenceObject(newKey);
        return kCfNameCollision;
    }

    Dsa_AvlInsert(&parentKey->SubKeys, &newKey->SiblingNode, s_SubKeyCmp);
    Ob_ReferenceObject(newKey);
    Core_SpinlockRelease(&parentKey->Lock);

    status = Ob_InsertHandle(table, newKey, desiredAccess, token, outHandle);
    if (status != kObSuccess)
    {
        Core_SpinlockAcquire(&parentKey->Lock);
        Dsa_AvlRemove(&parentKey->SubKeys, newKey->Name, s_SubKeyFind);
        Core_SpinlockRelease(&parentKey->Lock);

        Ob_DereferenceObject(newKey);
        Ob_DereferenceObject(parentKey);
        Ob_DereferenceObject(newKey);
        return kCfAccessDenied;
    }

    Ob_DereferenceObject(newKey);
    Ob_DereferenceObject(parentKey);
    return kCfSuccess;
}

i32 Cf_OpenKey(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *subPath, u32 desiredAccess,
               Ob_Handle *outHandle)
{
    if (!table || !subPath || !outHandle)
        return kCfInvalidParameter;

    Cf_Key *parentKey;
    i32     status = s_ResolveKey(table, parent, &parentKey);
    if (status != kCfSuccess)
        return status;

    Cf_Key *target = s_WalkPath(parentKey, subPath);
    Ob_DereferenceObject(parentKey);

    if (!target)
        return kCfNotFound;

    if (target->Flags & CF_KEY_FLAG_DELETED)
    {
        Ob_DereferenceObject(target);
        return kCfNotFound;
    }

    status = Ob_InsertHandle(table, target, desiredAccess, token, outHandle);
    Ob_DereferenceObject(target);

    return (status == kObSuccess) ? kCfSuccess : kCfAccessDenied;
}

i32 Cf_DeleteKey(Ob_HandleTable *table, Ob_Handle keyHandle)
{
    if (!table)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_DELETE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    Core_SpinlockAcquire(&key->Lock);
    if (key->SubKeys.Count > 0)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfNotEmpty;
    }
    key->Flags |= CF_KEY_FLAG_DELETED;
    Core_SpinlockRelease(&key->Lock);

    if (key->Parent)
    {
        Core_SpinlockAcquire(&key->Parent->Lock);
        Dsa_AvlRemove(&key->Parent->SubKeys, key->Name, s_SubKeyFind);
        Core_SpinlockRelease(&key->Parent->Lock);
    }

    if (key->Mount && key->Mount->Ops->DeleteKey)
        key->Mount->Ops->DeleteKey(key->Mount, key);

    Ob_DereferenceObject(key);
    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_SetValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name, Cf_ValueType type, const void *data,
                usize dataSize)
{
    if (!table || !name || (!data && dataSize > 0))
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_SET_VALUE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    void *dataCopy = nullptr;
    if (dataSize > 0)
    {
        dataCopy = Ex_Allocate(dataSize, EX_TAG_CFDATA);
        if (!dataCopy)
        {
            Ob_DereferenceObject(key);
            return kCfInsufficientResources;
        }

        Core_CopyMemory(dataCopy, data, dataSize);
    }

    Core_SpinlockAcquire(&key->Lock);

    Cf_Value *existing = s_FindValue(key, name);
    if (existing)
    {
        if (existing->Data)
            Ex_Free(existing->Data);

        existing->Type     = type;
        existing->Data     = dataCopy;
        existing->DataSize = dataSize;
    }
    else
    {
        Cf_Value *val = Ex_Allocate(sizeof *val, EX_TAG_CFVAL);
        if (!val)
        {
            Core_SpinlockRelease(&key->Lock);
            if (dataCopy)
                Ex_Free(dataCopy);

            Ob_DereferenceObject(key);
            return kCfInsufficientResources;
        }

        val->Name = Core_DuplicateString(name, EX_TAG_CFVAL);
        if (!val->Name)
        {
            Ex_Free(val);
            Core_SpinlockRelease(&key->Lock);
            if (dataCopy)
                Ex_Free(dataCopy);

            Ob_DereferenceObject(key);
            return kCfInsufficientResources;
        }

        val->Type     = type;
        val->Data     = dataCopy;
        val->DataSize = dataSize;

        Dsa_AvlInsert(&key->Values, &val->TreeNode, s_ValueCmp);
    }

    key->Flags |= CF_KEY_FLAG_DIRTY;
    Core_SpinlockRelease(&key->Lock);

    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_QueryValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name, Cf_ValueType *outType, void *buffer,
                  usize *inOutSize)
{
    if (!table || !name || !inOutSize)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_QUERY_VALUE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    Core_SpinlockAcquire(&key->Lock);

    Cf_Value *val = s_FindValue(key, name);
    if (!val)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfNotFound;
    }

    if (outType)
        *outType = val->Type;

    if (val->DataSize > *inOutSize)
    {
        *inOutSize = val->DataSize;
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfBufferTooSmall;
    }

    if (buffer && val->DataSize > 0)
        Core_CopyMemory(buffer, val->Data, val->DataSize);

    *inOutSize = val->DataSize;

    Core_SpinlockRelease(&key->Lock);
    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_DeleteValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name)
{
    if (!table || !name)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_SET_VALUE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    Core_SpinlockAcquire(&key->Lock);

    Dsa_AvlNode *removed = Dsa_AvlRemove(&key->Values, name, s_ValueFind);
    if (!removed)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfNotFound;
    }

    key->Flags |= CF_KEY_FLAG_DIRTY;
    Core_SpinlockRelease(&key->Lock);

    s_FreeValue(Dsa_AvlEntry(removed, Cf_Value, TreeNode));

    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_EnumerateSubKeys(Ob_HandleTable *table, Ob_Handle keyHandle, u32 index, char *nameBuffer, usize nameBufferSize)
{
    if (!table || !nameBuffer || nameBufferSize == 0)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_ENUMERATE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    Core_SpinlockAcquire(&key->Lock);

    Dsa_AvlNode *node = Dsa_AvlFirst(&key->SubKeys);
    u32          i    = 0;
    while (node && i < index)
    {
        node = Dsa_AvlNext(node);
        i++;
    }

    if (!node)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfNotFound;
    }

    Cf_Key *child = Dsa_AvlEntry(node, Cf_Key, SiblingNode);
    usize   len   = Core_StringLength(child->Name);
    if (len + 1 > nameBufferSize)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfBufferTooSmall;
    }

    Core_CopyString(nameBuffer, child->Name);

    Core_SpinlockRelease(&key->Lock);
    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_EnumerateValues(Ob_HandleTable *table, Ob_Handle keyHandle, u32 index, char *nameBuffer, usize nameBufferSize,
                       Cf_ValueType *outType)
{
    if (!table || !nameBuffer || nameBufferSize == 0)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    status = Ob_CheckHandleAccess(table, keyHandle, CF_KEY_ENUMERATE);
    if (status != kObSuccess)
    {
        Ob_DereferenceObject(key);
        return kCfAccessDenied;
    }

    Core_SpinlockAcquire(&key->Lock);

    Dsa_AvlNode *node = Dsa_AvlFirst(&key->Values);
    u32          i    = 0;
    while (node && i < index)
    {
        node = Dsa_AvlNext(node);
        i++;
    }

    if (!node)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfNotFound;
    }

    Cf_Value *val = Dsa_AvlEntry(node, Cf_Value, TreeNode);
    usize     len = Core_StringLength(val->Name);
    if (len + 1 > nameBufferSize)
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfBufferTooSmall;
    }

    Core_CopyString(nameBuffer, val->Name);
    if (outType)
        *outType = val->Type;

    Core_SpinlockRelease(&key->Lock);
    Ob_DereferenceObject(key);
    return kCfSuccess;
}

i32 Cf_FlushKey(Ob_HandleTable *table, Ob_Handle keyHandle)
{
    if (!table)
        return kCfInvalidParameter;

    Cf_Key *key;
    i32     status = s_ResolveKey(table, keyHandle, &key);
    if (status != kCfSuccess)
        return status;

    Core_SpinlockAcquire(&key->Lock);

    if (!(key->Flags & CF_KEY_FLAG_DIRTY))
    {
        Core_SpinlockRelease(&key->Lock);
        Ob_DereferenceObject(key);
        return kCfSuccess;
    }

    Cf_Mount *mount = key->Mount;
    Core_SpinlockRelease(&key->Lock);

    i32 result = kCfSuccess;
    if (mount && mount->Ops->FlushKey)
        result = mount->Ops->FlushKey(mount, key);

    if (result == kCfSuccess)
    {
        Core_SpinlockAcquire(&key->Lock);
        key->Flags &= ~CF_KEY_FLAG_DIRTY;
        Core_SpinlockRelease(&key->Lock);
    }

    Ob_DereferenceObject(key);
    return result;
}

Cf_Mount *Cf_CreateMount(const Cf_MountOps *ops, void *context, u32 flags)
{
    if (!ops)
        return nullptr;

    Cf_Mount *mount = Ex_Allocate(sizeof *mount, EX_TAG_CFMNT);
    if (!mount)
        return nullptr;

    mount->Ops     = ops;
    mount->Context = context;
    mount->Flags   = flags;
    return mount;
}

void Cf_DestroyMount(Cf_Mount *mount)
{
    if (!mount)
        return;

    if (mount->Ops->Unload)
        mount->Ops->Unload(mount);

    Ex_Free(mount);
}

i32 Cf_AttachMount(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *subKeyName, Cf_Mount *mount,
                   u32 desiredAccess, Ob_Handle *outHandle)
{
    if (!table || !subKeyName || !mount || !outHandle)
        return kCfInvalidParameter;

    Cf_Key *parentKey;
    i32     status = s_ResolveKey(table, parent, &parentKey);
    if (status != kCfSuccess)
        return status;

    if (parent != OB_HANDLE_NULL)
    {
        i32 accessStatus = Ob_CheckHandleAccess(table, parent, CF_KEY_CREATE_SUBKEY);
        if (accessStatus != kObSuccess)
        {
            Ob_DereferenceObject(parentKey);
            return kCfAccessDenied;
        }
    }

    Core_SpinlockAcquire(&parentKey->Lock);
    if (s_FindSubKey(parentKey, subKeyName))
    {
        Core_SpinlockRelease(&parentKey->Lock);
        Ob_DereferenceObject(parentKey);
        return kCfNameCollision;
    }
    Core_SpinlockRelease(&parentKey->Lock);

    Cf_Key *newKey;
    status = s_AllocateKey(parentKey, subKeyName, mount, &newKey);
    if (status != kCfSuccess)
    {
        Ob_DereferenceObject(parentKey);
        return status;
    }

    Core_SpinlockAcquire(&parentKey->Lock);
    if (s_FindSubKey(parentKey, subKeyName))
    {
        Core_SpinlockRelease(&parentKey->Lock);
        Ob_DereferenceObject(parentKey);
        Ob_DereferenceObject(newKey);
        return kCfNameCollision;
    }
    Dsa_AvlInsert(&parentKey->SubKeys, &newKey->SiblingNode, s_SubKeyCmp);
    Ob_ReferenceObject(newKey);
    Core_SpinlockRelease(&parentKey->Lock);

    status = Ob_InsertHandle(table, newKey, desiredAccess, token, outHandle);
    if (status != kObSuccess)
    {
        Core_SpinlockAcquire(&parentKey->Lock);
        Dsa_AvlRemove(&parentKey->SubKeys, newKey->Name, s_SubKeyFind);
        Core_SpinlockRelease(&parentKey->Lock);

        Ob_DereferenceObject(parentKey);
        Ob_DereferenceObject(newKey);
        return kCfAccessDenied;
    }

    Ob_DereferenceObject(newKey);
    Ob_DereferenceObject(parentKey);
    return kCfSuccess;
}

Ob_Type *Cf_GetKeyType(void)
{
    return s_KeyType;
}

Cf_Key *Cf_GetRootKey(void)
{
    return s_RootKey;
}

void Cf_Init(void)
{
    Ob_TypeInfo info = {
        .Name            = "CfKey",
        .ObjectBodySize  = sizeof(Cf_Key),
        .DeleteProcedure = s_KeyDelete,
        .CloseProcedure  = s_KeyClose,
        .PoolTag         = EX_TAG_CFKEY,
        .ValidAccessMask = CF_KEY_ALL_ACCESS,
    };

    s_KeyType = Ob_CreateType(&info);

    void *body;
    Ob_CreateObject(s_KeyType, 0, &body);
    s_RootKey = body;

    s_RootKey->Parent = nullptr;
    s_RootKey->Name   = Core_DuplicateString("CfDb", EX_TAG_CFKEY);
    s_RootKey->Mount  = &s_VolatileMount;
    s_RootKey->Flags  = 0;

    Dsa_AvlTreeInit(&s_RootKey->SubKeys);
    Dsa_AvlTreeInit(&s_RootKey->Values);
    Core_SpinlockInit(&s_RootKey->Lock);

    Acl_Acl *rootAcl = Acl_CreateSimple(ACL_SID_SUPERVISOR, CF_KEY_ALL_ACCESS, CF_KEY_QUERY_VALUE | CF_KEY_ENUMERATE);
    Ob_SetObjectSecurity(s_RootKey, ACL_SID_SUPERVISOR, rootAcl);

    Ob_InsertObjectByPath("\\CfDb", s_RootKey);
}
