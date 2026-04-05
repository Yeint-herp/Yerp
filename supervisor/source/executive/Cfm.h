#ifndef SUPERVISOR_EXECUTIVE_CFM_H
#define SUPERVISOR_EXECUTIVE_CFM_H

#include <core/Spinlock.h>
#include <dsa/Avl.h>
#include <executive/Acl.h>
#include <executive/Object.h>
#include <executive/Pool.h>

enum
{
    kCfSuccess,
    kCfInvalidParameter,
    kCfInsufficientResources,
    kCfNotFound,
    kCfNameCollision,
    kCfAccessDenied,
    kCfBufferTooSmall,
    kCfNotEmpty,
};

#define EX_TAG_CFKEY  EX_TAG('C', 'f', 'K', 'y')
#define EX_TAG_CFVAL  EX_TAG('C', 'f', 'V', 'l')
#define EX_TAG_CFMNT  EX_TAG('C', 'f', 'M', 't')
#define EX_TAG_CFDATA EX_TAG('C', 'f', 'D', 'a')

#define CF_KEY_QUERY_VALUE   (1u << 0)
#define CF_KEY_SET_VALUE     (1u << 1)
#define CF_KEY_CREATE_SUBKEY (1u << 2)
#define CF_KEY_ENUMERATE     (1u << 3)
#define CF_KEY_DELETE        (1u << 4)

#define CF_KEY_ALL_ACCESS                                                                                              \
    (CF_KEY_QUERY_VALUE | CF_KEY_SET_VALUE | CF_KEY_CREATE_SUBKEY | CF_KEY_ENUMERATE | CF_KEY_DELETE)

typedef enum Cf_ValueType
{
    kCfTypeNone,
    kCfTypeU32,
    kCfTypeU64,
    kCfTypeString,
    kCfTypeBinary,
    kCfTypeMultiString,
} Cf_ValueType;

typedef struct Cf_Value
{
    Dsa_AvlNode  TreeNode;
    char        *Name;
    Cf_ValueType Type;
    void        *Data;
    usize        DataSize;
} Cf_Value;

typedef struct Cf_Key   Cf_Key;
typedef struct Cf_Mount Cf_Mount;

typedef struct Cf_MountOps
{
    i32 (*FlushKey)(Cf_Mount *mount, Cf_Key *key);
    i32 (*DeleteKey)(Cf_Mount *mount, Cf_Key *key);
    void (*Unload)(Cf_Mount *mount);
} Cf_MountOps;

#define CF_MOUNT_VOLATILE (1u << 0)

struct Cf_Mount
{
    const Cf_MountOps *Ops;
    void              *Context;
    u32                Flags;
};

#define CF_KEY_FLAG_DIRTY   (1u << 0)
#define CF_KEY_FLAG_DELETED (1u << 1)

struct Cf_Key
{
    Cf_Key *Parent;
    char   *Name;

    Dsa_AvlNode SiblingNode;
    Dsa_AvlTree SubKeys;
    Dsa_AvlTree Values;

    Cf_Mount *Mount;
    u32       Flags;

    Core_Spinlock Lock;
};

i32 Cf_CreateKey(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *name, u32 desiredAccess,
                 Ob_Handle *outHandle);

i32 Cf_OpenKey(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *subPath, u32 desiredAccess,
               Ob_Handle *outHandle);

i32 Cf_DeleteKey(Ob_HandleTable *table, Ob_Handle keyHandle);

i32 Cf_SetValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name, Cf_ValueType type, const void *data,
                usize dataSize);

i32 Cf_QueryValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name, Cf_ValueType *outType, void *buffer,
                  usize *inOutSize);

i32 Cf_DeleteValue(Ob_HandleTable *table, Ob_Handle keyHandle, const char *name);

i32 Cf_EnumerateSubKeys(Ob_HandleTable *table, Ob_Handle keyHandle, u32 index, char *nameBuffer, usize nameBufferSize);

i32 Cf_EnumerateValues(Ob_HandleTable *table, Ob_Handle keyHandle, u32 index, char *nameBuffer, usize nameBufferSize,
                       Cf_ValueType *outType);

i32 Cf_FlushKey(Ob_HandleTable *table, Ob_Handle keyHandle);

Cf_Mount *Cf_CreateMount(const Cf_MountOps *ops, void *context, u32 flags);
void      Cf_DestroyMount(Cf_Mount *mount);

i32 Cf_AttachMount(Ob_HandleTable *table, Acl_Token *token, Ob_Handle parent, const char *subKeyName, Cf_Mount *mount,
                   u32 desiredAccess, Ob_Handle *outHandle);

Ob_Type *Cf_GetKeyType(void);
Cf_Key  *Cf_GetRootKey(void);

void Cf_Init(void);

#endif /* SUPERVISOR_EXECUTIVE_CFM_H */
