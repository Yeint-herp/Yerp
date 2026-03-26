#ifdef CI_BUILD

#include <executive/Object.h>
#include <mm/Pool.h>
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
    i32       status = Ob_InsertHandle(&table, obj, 0x3, &h);
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
    Ob_InsertHandle(&table, obj, 0x1, &h1);
    Ob_InsertHandle(&table, obj, 0x2, &h2);

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
    Ob_InsertHandle(&table, obj, 0x1, &h);

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
        i32 status = Ob_InsertHandle(&table, obj, i, &handles[i]);
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
    Ob_InsertHandle(&table, obj, 0x1, &h1);
    Ob_CloseHandle(&table, h1);

    Ob_InsertHandle(&table, obj, 0x2, &h2);
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

#endif /* CI_BUILD */
