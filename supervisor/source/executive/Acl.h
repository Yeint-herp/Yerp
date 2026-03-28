#ifndef SUPERVISOR_EXECUTIVE_ACL_H
#define SUPERVISOR_EXECUTIVE_ACL_H

#include <arch/Atomic.h>

typedef u32 Acl_Sid;

#define ACL_SID_SUPERVISOR 0
#define ACL_SID_EVERYONE   1
#define ACL_SID_NOBODY     ((Acl_Sid) - 1)

#define ACL_PRIV_BYPASS_ACL    (1u << 0)
#define ACL_PRIV_CREATE_TOKEN  (1u << 1)
#define ACL_PRIV_DEBUG_PROCESS (1u << 2)
#define ACL_PRIV_TAKE_OWNER    (1u << 3)

typedef struct Acl_Token
{
    Acl_Sid  Owner;
    Acl_Sid *Groups;
    u32      GroupCount;
    u32      Privileges;

    Arch_Atomic32 ReferenceCount;
} Acl_Token;

i32  Acl_CreateToken(Acl_Sid owner, const Acl_Sid *groups, u32 groupCount, u32 privileges, Acl_Token **outToken);
void Acl_ReferenceToken(Acl_Token *token);
bool Acl_DereferenceToken(Acl_Token *token);

bool Acl_HasPrivilege(const Acl_Token *token, u32 privilege);
bool Acl_TokenMatchesSid(const Acl_Token *token, Acl_Sid sid);

typedef enum Acl_AceType
{
    ACL_ACE_ALLOW,
    ACL_ACE_DENY,
} Acl_AceType;

typedef struct Acl_Ace
{
    Acl_AceType Type;
    Acl_Sid     Sid;
    u32         AccessMask;
} Acl_Ace;

typedef struct Acl_Acl
{
    u32     Count;
    Acl_Ace Entries[];
} Acl_Acl;

Acl_Acl *Acl_CreateAcl(u32 count);
void     Acl_DestroyAcl(Acl_Acl *acl);

u32 Acl_ComputeAccess(const Acl_Acl *acl, const Acl_Token *token, u32 desiredAccess);

Acl_Acl *Acl_CreateSimple(Acl_Sid owner, u32 ownerAccess, u32 othersAccess);

#endif /* SUPERVISOR_EXECUTIVE_ACL_H */
