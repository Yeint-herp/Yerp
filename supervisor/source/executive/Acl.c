#define DBG_MODULE "Acl"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Acl.h>
#include <executive/Pool.h>

#define EX_TAG_ACLTKN EX_TAG('A', 'c', 'T', 'k')
#define EX_TAG_ACLACL EX_TAG('A', 'c', 'L', 'l')

i32 Acl_CreateToken(Acl_Sid owner, const Acl_Sid *groups, u32 groupCount, u32 privileges, Acl_Token **outToken)
{
    if (!outToken)
        return -1;

    Acl_Token *token = Ex_Allocate(sizeof *token, EX_TAG_ACLTKN);
    if (!token)
        return -1;

    token->Owner      = owner;
    token->Privileges = privileges;
    token->GroupCount = groupCount;
    token->Groups     = nullptr;

    if (groupCount > 0 && groups)
    {
        const usize sz = sizeof(Acl_Sid) * groupCount;
        token->Groups  = Ex_Allocate(sz, EX_TAG_ACLTKN);

        if (!token->Groups)
        {
            Ex_Free(token);
            return -1;
        }

        Core_CopyMemory(token->Groups, groups, sz);
    }

    token->ReferenceCount = 1;
    *outToken             = token;

    return 0;
}

void Acl_ReferenceToken(Acl_Token *token)
{
    i32 old = Arch_AtomicAdd32(&token->ReferenceCount, 1);

    if (old <= 0)
        Panic("Acl_ReferenceToken: referencing token with count %d", old);
}

bool Acl_DereferenceToken(Acl_Token *token)
{
    i32 old = Arch_AtomicSub32(&token->ReferenceCount, 1);

    if (old < 1)
        Panic("Acl_DereferenceToken: underflow (count was %d)", old);

    if (old == 1)
    {
        if (token->Groups)
            Ex_Free(token->Groups);

        Ex_Free(token);
        return true;
    }

    return false;
}

bool Acl_HasPrivilege(const Acl_Token *token, u32 privilege)
{
    return (token->Privileges & privilege) == privilege;
}

bool Acl_TokenMatchesSid(const Acl_Token *token, Acl_Sid sid)
{
    if (sid == ACL_SID_EVERYONE)
        return true;

    if (token->Owner == sid)
        return true;

    for (u32 i = 0; i < token->GroupCount; i++)
        if (token->Groups[i] == sid)
            return true;

    return false;
}

Acl_Acl *Acl_CreateAcl(u32 count)
{
    const usize size = sizeof(Acl_Acl) + sizeof(Acl_Ace) * count;
    Acl_Acl    *acl  = Ex_Allocate(size, EX_TAG_ACLACL);

    if (!acl)
        return nullptr;

    Core_ZeroMemory(acl, size);
    acl->Count = count;

    return acl;
}

void Acl_DestroyAcl(Acl_Acl *acl)
{
    if (acl)
        Ex_Free(acl);
}

u32 Acl_ComputeAccess(const Acl_Acl *acl, const Acl_Token *token, u32 desiredAccess)
{
    if (Acl_HasPrivilege(token, ACL_PRIV_BYPASS_ACL))
        return desiredAccess;

    if (!acl)
        return desiredAccess;

    u32 allowed = 0;
    u32 denied  = 0;

    for (u32 i = 0; i < acl->Count; i++)
    {
        const Acl_Ace *ace = &acl->Entries[i];

        if (!Acl_TokenMatchesSid(token, ace->Sid))
            continue;

        if (ace->Type == ACL_ACE_DENY)
            denied |= ace->AccessMask;
        else
            allowed |= ace->AccessMask;
    }

    return (allowed & ~denied) & desiredAccess;
}

Acl_Acl *Acl_CreateSimple(Acl_Sid owner, u32 ownerAccess, u32 othersAccess)
{
    u32 count = 1;

    if (othersAccess)
        count = 2;

    Acl_Acl *acl = Acl_CreateAcl(count);
    if (!acl)
        return nullptr;

    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = owner;
    acl->Entries[0].AccessMask = ownerAccess;

    if (othersAccess)
    {
        acl->Entries[1].Type       = ACL_ACE_ALLOW;
        acl->Entries[1].Sid        = ACL_SID_EVERYONE;
        acl->Entries[1].AccessMask = othersAccess;
    }

    return acl;
}
