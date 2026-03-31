#ifdef CI_BUILD

#include <executive/Acl.h>
#include <executive/Object.h>
#include <executive/Pool.h>
#include <tests/CiTest.h>

#define TEST_TAG EX_TAG('T', 's', 'O', 'b')

typedef struct
{
    u64 Value;
    u32 DeleteCount;
} TestBody;

static u32   s_DeleteCalls     = 0;
static u32   s_CloseCalls      = 0;
static usize s_LastHandleCount = 0;

static void TestDeleteProc(void *object)
{
    TestBody *body = (TestBody *)object;
    body->DeleteCount++;
    s_DeleteCalls++;
}

static void TestCloseProc(void *, usize handleCount)
{
    s_CloseCalls++;
    s_LastHandleCount = handleCount;
}

static void ResetCounters(void)
{
    s_DeleteCalls     = 0;
    s_CloseCalls      = 0;
    s_LastHandleCount = 0;
}

static Ob_Type *CreateTestType(const char *name)
{
    Ob_TypeInfo info = {
        .Name            = name,
        .ObjectBodySize  = sizeof(TestBody),
        .DeleteProcedure = TestDeleteProc,
        .CloseProcedure  = TestCloseProc,
        .PoolTag         = TEST_TAG,
        .ValidAccessMask = 0xF,
    };

    return Ob_CreateType(&info);
}

static Acl_Token *CreateSystemToken(void)
{
    Acl_Token *token = nullptr;
    Acl_CreateToken(ACL_SID_SUPERVISOR, nullptr, 0, ACL_PRIV_BYPASS_ACL, &token);
    return token;
}

static Acl_Token *CreateUserToken(Acl_Sid owner, u32 privileges)
{
    Acl_Token *token = nullptr;
    Acl_CreateToken(owner, nullptr, 0, privileges, &token);
    return token;
}

static Acl_Token *CreateUserTokenWithGroups(Acl_Sid owner, const Acl_Sid *groups, u32 groupCount, u32 privileges)
{
    Acl_Token *token = nullptr;
    Acl_CreateToken(owner, groups, groupCount, privileges, &token);
    return token;
}

CI_TEST("ob: type registration", TestTypeCreate)
{
    Ob_Type *type = CreateTestType("TestTypeA");
    if (!type)
        return false;

    if (type->Info.ObjectBodySize != sizeof(TestBody))
        return false;

    if (type->TotalObjects != 0)
        return false;

    return true;
}

CI_TEST("ob: create and destroy", TestCreateDestroy)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestLifecycle");

    TestBody *obj    = nullptr;
    i32       status = Ob_CreateObject(type, 0, (void **)&obj);
    if (status != OB_SUCCESS || !obj)
        return false;

    if (Ob_GetReferenceCount(obj) != 1)
        return false;

    if (Ob_GetObjectType(obj) != type)
        return false;

    bool destroyed = Ob_DereferenceObject(obj);
    if (!destroyed)
        return false;

    if (s_DeleteCalls != 1)
        return false;

    return true;
}

CI_TEST("ob: reference counting", TestRefCount)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestRefCount");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_ReferenceObject(obj);
    Ob_ReferenceObject(obj);

    if (Ob_GetReferenceCount(obj) != 3)
        return false;

    Ob_DereferenceObject(obj);
    if (Ob_GetReferenceCount(obj) != 2)
        return false;

    Ob_DereferenceObject(obj);
    if (s_DeleteCalls != 0)
        return false;

    Ob_DereferenceObject(obj);
    if (s_DeleteCalls != 1)
        return false;

    return true;
}

CI_TEST("ob: zero body init", TestZeroInit)
{
    Ob_Type *type = CreateTestType("TestZeroInit");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    if (obj->Value != 0 || obj->DeleteCount != 0)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("ob: handle insert and reference", TestHandleBasic)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestHandle");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h      = OB_HANDLE_NULL;
    i32       status = Ob_InsertHandleRaw(&table, obj, 0x3, &h);
    if (status != OB_SUCCESS || h == OB_HANDLE_NULL)
    {
        Ob_DereferenceObject(obj);
        Ob_DestroyHandleTable(&table);
        return false;
    }

    if (Ob_GetReferenceCount(obj) != 2)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }
    if (Ob_GetHandleCount(obj) != 1)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    TestBody *looked = nullptr;
    status           = Ob_ReferenceByHandle(&table, h, type, (void **)&looked);
    if (status != OB_SUCCESS || looked != obj)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    if (Ob_GetReferenceCount(obj) != 3)
    {
        Ob_DereferenceObject(looked);
        Ob_DestroyHandleTable(&table);
        return false;
    }

    Ob_DereferenceObject(looked);

    if (Ob_GetHandleAccess(&table, h) != 0x3)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    if (s_DeleteCalls != 1)
        return false;

    return true;
}

CI_TEST("ob: handle close invokes close proc", TestHandleClose)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestHandleClose");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h1, h2;
    Ob_InsertHandleRaw(&table, obj, 0x1, &h1);
    Ob_InsertHandleRaw(&table, obj, 0x2, &h2);

    if (Ob_GetHandleCount(obj) != 2)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    Ob_CloseHandle(&table, h1);
    if (s_CloseCalls != 1 || s_LastHandleCount != 1)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);

    if (s_CloseCalls != 2)
        return false;
    if (s_DeleteCalls != 1)
        return false;

    return true;
}

CI_TEST("ob: handle type mismatch", TestHandleTypeMismatch)
{
    Ob_Type *typeA = CreateTestType("TestTypeCheckA");
    Ob_Type *typeB = CreateTestType("TestTypeCheckB");

    TestBody *obj = nullptr;
    Ob_CreateObject(typeA, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h;
    Ob_InsertHandleRaw(&table, obj, 0x1, &h);

    TestBody *result = nullptr;
    i32       status = Ob_ReferenceByHandle(&table, h, typeB, (void **)&result);
    if (status != OB_TYPE_MISMATCH)
    {
        Ob_DestroyHandleTable(&table);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    return true;
}

CI_TEST("ob: handle table growth", TestHandleGrowth)
{
    Ob_Type *type = CreateTestType("TestGrowth");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle handles[OB_HANDLE_TABLE_INITIAL_SIZE + 16];
    for (u32 i = 0; i < OB_HANDLE_TABLE_INITIAL_SIZE + 16; i++)
    {
        i32 status = Ob_InsertHandleRaw(&table, obj, i & 0xF, &handles[i]);
        if (status != OB_SUCCESS)
        {
            Ob_DestroyHandleTable(&table);
            Ob_DereferenceObject(obj);
            return false;
        }
    }

    if (table.Capacity <= OB_HANDLE_TABLE_INITIAL_SIZE)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    for (u32 i = 0; i < OB_HANDLE_TABLE_INITIAL_SIZE + 16; i++)
    {
        TestBody *result = nullptr;
        i32       status = Ob_ReferenceByHandle(&table, handles[i], type, (void **)&result);
        if (status != OB_SUCCESS || result != obj)
        {
            Ob_DestroyHandleTable(&table);
            Ob_DereferenceObject(obj);
            return false;
        }
        Ob_DereferenceObject(result);
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("ob: handle reuse after close", TestHandleReuse)
{
    Ob_Type *type = CreateTestType("TestReuse");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h1, h2;
    Ob_InsertHandleRaw(&table, obj, 0x1, &h1);
    Ob_CloseHandle(&table, h1);

    Ob_InsertHandleRaw(&table, obj, 0x2, &h2);
    if (h2 != h1)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("ob: directory insert and lookup", TestDirBasic)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestDirType");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    obj->Value = 0xBEEF;

    Ob_Directory dir;
    Core_SpinlockInit(&dir.Lock);
    for (u32 i = 0; i < OB_DIRECTORY_BUCKETS; i++)
        dir.Buckets[i] = nullptr;

    i32 status = Ob_InsertObject(&dir, obj, "TestEntry");
    if (status != OB_SUCCESS)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    if (Ob_GetReferenceCount(obj) != 2)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    TestBody *found = nullptr;
    status          = Ob_LookupObject(&dir, "TestEntry", type, (void **)&found);
    if (status != OB_SUCCESS || found != obj || found->Value != 0xBEEF)
    {
        Ob_DereferenceObject(obj);
        return false;
    }
    Ob_DereferenceObject(found);

    TestBody *obj2 = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj2);
    status = Ob_InsertObject(&dir, obj2, "TestEntry");
    if (status != OB_NAME_COLLISION)
    {
        Ob_DereferenceObject(obj);
        Ob_DereferenceObject(obj2);
        return false;
    }
    Ob_DereferenceObject(obj2);

    status = Ob_RemoveObject(&dir, "TestEntry");
    if (status != OB_SUCCESS)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    status = Ob_LookupObject(&dir, "TestEntry", nullptr, (void **)&found);
    if (status != OB_NOT_FOUND)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    bool destroyed = Ob_DereferenceObject(obj);
    if (!destroyed || s_DeleteCalls != 2)
        return false;

    return true;
}

CI_TEST("ob: directory type mismatch", TestDirTypeMismatch)
{
    Ob_Type *typeA = CreateTestType("TestDirA");
    Ob_Type *typeB = CreateTestType("TestDirB");

    TestBody *obj = nullptr;
    Ob_CreateObject(typeA, 0, (void **)&obj);

    Ob_Directory dir;
    Core_SpinlockInit(&dir.Lock);
    for (u32 i = 0; i < OB_DIRECTORY_BUCKETS; i++)
        dir.Buckets[i] = nullptr;

    Ob_InsertObject(&dir, obj, "TypedEntry");

    TestBody *found  = nullptr;
    i32       status = Ob_LookupObject(&dir, "TypedEntry", typeB, (void **)&found);
    if (status != OB_TYPE_MISMATCH)
    {
        Ob_RemoveObject(&dir, "TypedEntry");
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_RemoveObject(&dir, "TypedEntry");
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("ob: path-based namespace", TestNamespace)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestNsType");

    Ob_Directory *sub    = nullptr;
    i32           status = Ob_CreateDirectory("\\TestNs", &sub);
    if (status != OB_SUCCESS)
        return false;

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    obj->Value = 0xCAFE;

    status = Ob_InsertObjectByPath("\\TestNs\\Widget", obj);
    if (status != OB_SUCCESS)
    {
        Ob_DereferenceObject(obj);
        Ob_DereferenceObject(sub);
        return false;
    }

    TestBody *found = nullptr;
    status          = Ob_LookupObjectByPath("\\TestNs\\Widget", type, (void **)&found);
    if (status != OB_SUCCESS || found->Value != 0xCAFE)
    {
        Ob_DereferenceObject(obj);
        Ob_DereferenceObject(sub);
        return false;
    }
    Ob_DereferenceObject(found);

    status = Ob_RemoveObjectByPath("\\TestNs\\Widget");
    if (status != OB_SUCCESS)
    {
        Ob_DereferenceObject(obj);
        Ob_DereferenceObject(sub);
        return false;
    }

    status = Ob_LookupObjectByPath("\\TestNs\\Widget", nullptr, (void **)&found);
    if (status != OB_NOT_FOUND)
    {
        Ob_DereferenceObject(obj);
        Ob_DereferenceObject(sub);
        return false;
    }

    Ob_DereferenceObject(obj);
    Ob_RemoveObjectByPath("\\TestNs");
    Ob_DereferenceObject(sub);

    return true;
}

CI_TEST("acl: token create and destroy", TestTokenLifecycle)
{
    Acl_Token *token  = nullptr;
    i32        status = Acl_CreateToken(100, nullptr, 0, 0, &token);
    if (status != 0 || !token)
        return false;

    if (token->Owner != 100)
        return false;

    if (token->GroupCount != 0 || token->Groups != nullptr)
        return false;

    bool freed = Acl_DereferenceToken(token);
    if (!freed)
        return false;

    return true;
}

CI_TEST("acl: token with groups", TestTokenGroups)
{
    Acl_Sid groups[] = {10, 20, 30};

    Acl_Token *token = nullptr;
    Acl_CreateToken(5, groups, 3, 0, &token);

    if (token->GroupCount != 3)
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (!Acl_TokenMatchesSid(token, 5))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (!Acl_TokenMatchesSid(token, 20))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (Acl_TokenMatchesSid(token, 99))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (!Acl_TokenMatchesSid(token, ACL_SID_EVERYONE))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    Acl_DereferenceToken(token);
    return true;
}

CI_TEST("acl: token refcounting", TestTokenRefCount)
{
    Acl_Token *token = nullptr;
    Acl_CreateToken(1, nullptr, 0, 0, &token);

    Acl_ReferenceToken(token);
    Acl_ReferenceToken(token);

    if (Acl_DereferenceToken(token))
        return false;

    if (Acl_DereferenceToken(token))
        return false;

    if (!Acl_DereferenceToken(token))
        return false;

    return true;
}

CI_TEST("acl: privilege check", TestPrivilegeCheck)
{
    Acl_Token *token = CreateUserToken(1, ACL_PRIV_DEBUG_PROCESS | ACL_PRIV_TAKE_OWNER);

    if (!Acl_HasPrivilege(token, ACL_PRIV_DEBUG_PROCESS))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (!Acl_HasPrivilege(token, ACL_PRIV_TAKE_OWNER))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (Acl_HasPrivilege(token, ACL_PRIV_BYPASS_ACL))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    if (Acl_HasPrivilege(token, ACL_PRIV_DEBUG_PROCESS | ACL_PRIV_BYPASS_ACL))
    {
        Acl_DereferenceToken(token);
        return false;
    }

    Acl_DereferenceToken(token);
    return true;
}

CI_TEST("acl: null acl grants all", TestNullAclGrantsAll)
{
    Acl_Token *token = CreateUserToken(42, 0);

    u32 granted = Acl_ComputeAccess(nullptr, token, 0xF);
    Acl_DereferenceToken(token);

    return granted == 0xF;
}

CI_TEST("acl: bypass privilege grants all", TestBypassGrantsAll)
{
    Acl_Token *token = CreateSystemToken();

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_DENY;
    acl->Entries[0].Sid        = ACL_SID_SUPERVISOR;
    acl->Entries[0].AccessMask = 0xF;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0xF;
}

CI_TEST("acl: allow entry grants access", TestAclAllow)
{
    Acl_Token *token = CreateUserToken(42, 0);

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = 42;
    acl->Entries[0].AccessMask = 0x3;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0x3;
}

CI_TEST("acl: deny overrides allow", TestAclDenyOverride)
{
    Acl_Token *token = CreateUserToken(42, 0);

    Acl_Acl *acl               = Acl_CreateAcl(2);
    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = 42;
    acl->Entries[0].AccessMask = 0xF;

    acl->Entries[1].Type       = ACL_ACE_DENY;
    acl->Entries[1].Sid        = 42;
    acl->Entries[1].AccessMask = 0xC;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0x3;
}

CI_TEST("acl: non-matching sid gets nothing", TestAclNoMatch)
{
    Acl_Token *token = CreateUserToken(42, 0);

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = 99;
    acl->Entries[0].AccessMask = 0xF;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0;
}

CI_TEST("acl: group membership grants access", TestAclGroupMatch)
{
    Acl_Sid    groups[] = {100};
    Acl_Token *token    = CreateUserTokenWithGroups(42, groups, 1, 0);

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = 100;
    acl->Entries[0].AccessMask = 0x5;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0x5;
}

CI_TEST("acl: everyone sid matches any token", TestAclEveryoneMatch)
{
    Acl_Token *token = CreateUserToken(42, 0);

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_ALLOW;
    acl->Entries[0].Sid        = ACL_SID_EVERYONE;
    acl->Entries[0].AccessMask = 0x1;

    u32 granted = Acl_ComputeAccess(acl, token, 0xF);

    Acl_DestroyAcl(acl);
    Acl_DereferenceToken(token);

    return granted == 0x1;
}

CI_TEST("acl: simple acl creation", TestAclCreateSimple)
{
    Acl_Acl *acl = Acl_CreateSimple(42, 0xF, 0x1);
    if (!acl)
        return false;

    if (acl->Count != 2)
    {
        Acl_DestroyAcl(acl);
        return false;
    }

    Acl_Token *owner   = CreateUserToken(42, 0);
    u32        granted = Acl_ComputeAccess(acl, owner, 0xF);
    Acl_DereferenceToken(owner);

    if (granted != 0xF)
    {
        Acl_DestroyAcl(acl);
        return false;
    }

    Acl_Token *other = CreateUserToken(99, 0);
    granted          = Acl_ComputeAccess(acl, other, 0xF);
    Acl_DereferenceToken(other);

    if (granted != 0x1)
    {
        Acl_DestroyAcl(acl);
        return false;
    }

    Acl_DestroyAcl(acl);
    return true;
}

CI_TEST("acl: simple acl no others", TestAclCreateSimpleNoOthers)
{
    Acl_Acl *acl = Acl_CreateSimple(42, 0xF, 0);
    if (!acl)
        return false;

    if (acl->Count != 1)
    {
        Acl_DestroyAcl(acl);
        return false;
    }

    Acl_Token *other   = CreateUserToken(99, 0);
    u32        granted = Acl_ComputeAccess(acl, other, 0xF);
    Acl_DereferenceToken(other);

    Acl_DestroyAcl(acl);
    return granted == 0;
}

CI_TEST("acl: checked insert grants access", TestCheckedInsertAllow)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestAclHandle");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0x1));

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *owner  = CreateUserToken(42, 0);
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_InsertHandle(&table, obj, 0xF, owner, &h);
    Acl_DereferenceToken(owner);

    if (status != OB_SUCCESS || h == OB_HANDLE_NULL)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    if (Ob_GetHandleAccess(&table, h) != 0xF)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: checked insert denies excess access", TestCheckedInsertDeny)
{
    Ob_Type *type = CreateTestType("TestAclDeny");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0x1));

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *other  = CreateUserToken(99, 0);
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_InsertHandle(&table, obj, 0xF, other, &h);
    Acl_DereferenceToken(other);

    if (status != OB_ACCESS_DENIED)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: checked insert with subset access", TestCheckedInsertSubset)
{
    Ob_Type *type = CreateTestType("TestAclSubset");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0x3));

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *other  = CreateUserToken(99, 0);
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_InsertHandle(&table, obj, 0x1, other, &h);
    Acl_DereferenceToken(other);

    if (status != OB_SUCCESS)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    if (Ob_GetHandleAccess(&table, h) != 0x1)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: checked insert rejects invalid access bits", TestCheckedInsertInvalidBits)
{
    Ob_Type *type = CreateTestType("TestAclInvalid");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *token  = CreateSystemToken();
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_InsertHandle(&table, obj, 0xFF, token, &h);
    Acl_DereferenceToken(token);

    if (status != OB_INVALID_PARAMETER)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: bypass privilege skips acl on insert", TestCheckedInsertBypass)
{
    Ob_Type *type = CreateTestType("TestAclBypass");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Acl_Acl *acl               = Acl_CreateAcl(1);
    acl->Entries[0].Type       = ACL_ACE_DENY;
    acl->Entries[0].Sid        = ACL_SID_EVERYONE;
    acl->Entries[0].AccessMask = 0xF;
    Ob_SetObjectSecurity(obj, 42, acl);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *system = CreateSystemToken();
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_InsertHandle(&table, obj, 0xF, system, &h);
    Acl_DereferenceToken(system);

    if (status != OB_SUCCESS)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: check handle access pass", TestCheckHandleAccessPass)
{
    Ob_Type *type = CreateTestType("TestChkPass");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h;
    Ob_InsertHandleRaw(&table, obj, 0x7, &h);

    i32 status = Ob_CheckHandleAccess(&table, h, 0x3);
    if (status != OB_SUCCESS)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: check handle access fail", TestCheckHandleAccessFail)
{
    Ob_Type *type = CreateTestType("TestChkFail");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Ob_Handle h;
    Ob_InsertHandleRaw(&table, obj, 0x1, &h);

    i32 status = Ob_CheckHandleAccess(&table, h, 0x3);
    if (status != OB_ACCESS_DENIED)
    {
        Ob_DestroyHandleTable(&table);
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_DereferenceObject(obj);
    return true;
}

CI_TEST("acl: open by path allowed", TestOpenByPathAllow)
{
    ResetCounters();
    Ob_Type *type = CreateTestType("TestOpenAllow");

    Ob_Directory *sub = nullptr;
    Ob_CreateDirectory("\\TestOpen", &sub);

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    obj->Value = 0xABCD;
    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0x1));

    Ob_InsertObjectByPath("\\TestOpen\\Gadget", obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *owner  = CreateUserToken(42, 0);
    Ob_Handle  h      = OB_HANDLE_NULL;
    i32        status = Ob_OpenObjectByPath("\\TestOpen\\Gadget", type, 0xF, owner, &table, &h);
    Acl_DereferenceToken(owner);

    if (status != OB_SUCCESS || h == OB_HANDLE_NULL)
    {
        Ob_DestroyHandleTable(&table);
        Ob_RemoveObjectByPath("\\TestOpen\\Gadget");
        Ob_DereferenceObject(obj);
        Ob_RemoveObjectByPath("\\TestOpen");
        Ob_DereferenceObject(sub);
        return false;
    }

    TestBody *result = nullptr;
    status           = Ob_ReferenceByHandle(&table, h, type, (void **)&result);
    if (status != OB_SUCCESS || result->Value != 0xABCD)
    {
        Ob_DestroyHandleTable(&table);
        Ob_RemoveObjectByPath("\\TestOpen\\Gadget");
        Ob_DereferenceObject(obj);
        Ob_RemoveObjectByPath("\\TestOpen");
        Ob_DereferenceObject(sub);
        return false;
    }
    Ob_DereferenceObject(result);

    Ob_DestroyHandleTable(&table);
    Ob_RemoveObjectByPath("\\TestOpen\\Gadget");
    Ob_DereferenceObject(obj);
    Ob_RemoveObjectByPath("\\TestOpen");
    Ob_DereferenceObject(sub);
    return true;
}

CI_TEST("acl: open by path denied", TestOpenByPathDeny)
{
    Ob_Type *type = CreateTestType("TestOpenDeny");

    Ob_Directory *sub = nullptr;
    Ob_CreateDirectory("\\TestOpenD", &sub);

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);
    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0));

    Ob_InsertObjectByPath("\\TestOpenD\\Secret", obj);

    Ob_HandleTable table;
    Ob_InitHandleTable(&table);

    Acl_Token *outsider = CreateUserToken(99, 0);
    Ob_Handle  h        = OB_HANDLE_NULL;
    i32        status   = Ob_OpenObjectByPath("\\TestOpenD\\Secret", type, 0x1, outsider, &table, &h);
    Acl_DereferenceToken(outsider);

    if (status != OB_ACCESS_DENIED)
    {
        Ob_DestroyHandleTable(&table);
        Ob_RemoveObjectByPath("\\TestOpenD\\Secret");
        Ob_DereferenceObject(obj);
        Ob_RemoveObjectByPath("\\TestOpenD");
        Ob_DereferenceObject(sub);
        return false;
    }

    Ob_DestroyHandleTable(&table);
    Ob_RemoveObjectByPath("\\TestOpenD\\Secret");
    Ob_DereferenceObject(obj);
    Ob_RemoveObjectByPath("\\TestOpenD");
    Ob_DereferenceObject(sub);
    return true;
}

CI_TEST("acl: object security update", TestSetObjectSecurity)
{
    Ob_Type *type = CreateTestType("TestSecUpdate");

    TestBody *obj = nullptr;
    Ob_CreateObject(type, 0, (void **)&obj);

    Ob_Header *hdr = Ob_GetHeader(obj);
    if (hdr->Acl != nullptr || hdr->Owner != ACL_SID_SUPERVISOR)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_SetObjectSecurity(obj, 42, Acl_CreateSimple(42, 0xF, 0));

    if (hdr->Owner != 42 || hdr->Acl == nullptr)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_SetObjectSecurity(obj, 99, Acl_CreateSimple(99, 0x3, 0x1));

    if (hdr->Owner != 99)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    Acl_Token *owner   = CreateUserToken(99, 0);
    u32        granted = Acl_ComputeAccess(hdr->Acl, owner, 0xF);
    Acl_DereferenceToken(owner);

    if (granted != 0x3)
    {
        Ob_DereferenceObject(obj);
        return false;
    }

    Ob_DereferenceObject(obj);
    return true;
}

#endif /* CI_BUILD */
