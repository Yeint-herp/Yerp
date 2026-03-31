#ifdef CI_BUILD

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <mm/PfnDb.h>
#include <executive/Pool.h>
#include <tests/CiTest.h>

CI_TEST("pool: small allocation returns non-null", PoolSmallAlloc)
{
    void *p = Ex_Allocate(64, EX_TAG('T', 'e', 's', 't'));
    if (!p)
    {
        Dbg_Print("FAIL: Ex_Allocate(64) returned null\n");
        return false;
    }

    Ex_Free(p);
    return true;
}

CI_TEST("pool: allocation is zeroed", PoolAllocZeroed)
{
    void *p = Ex_Allocate(128, EX_TAG('T', 'e', 's', 't'));
    if (!p)
    {
        Dbg_Print("FAIL: Ex_Allocate(128) returned null\n");
        return false;
    }

    u8  *bytes   = (u8 *)p;
    bool allZero = true;
    for (usize i = 0; i < 128; i++)
    {
        if (bytes[i] != 0)
        {
            Dbg_Print("FAIL: byte[%zu] = 0x%x, expected 0\n", i, bytes[i]);
            allZero = false;
            break;
        }
    }

    Ex_Free(p);
    return allZero;
}

CI_TEST("pool: zero-size allocation returns null", PoolZeroSize)
{
    void *p = Ex_Allocate(0, EX_TAG('T', 'e', 's', 't'));
    if (p)
    {
        Dbg_Print("FAIL: Ex_Allocate(0) returned non-null\n");
        Ex_Free(p);
        return false;
    }

    return true;
}

CI_TEST("pool: distinct allocations don't overlap", PoolNoOverlap)
{
    void *a = Ex_Allocate(64, EX_TAG('T', 'e', 's', 't'));
    void *b = Ex_Allocate(64, EX_TAG('T', 'e', 's', 't'));

    if (!a || !b)
    {
        Dbg_Print("FAIL: allocation returned null\n");
        if (a)
            Ex_Free(a);
        if (b)
            Ex_Free(b);
        return false;
    }

    if (a == b)
    {
        Dbg_Print("FAIL: two allocations returned same pointer\n");
        Ex_Free(a);
        return false;
    }

    Core_FillMemory(a, 0xAA, 64);
    Core_FillMemory(b, 0x55, 64);

    u8 *ba = (u8 *)a;
    u8 *bb = (u8 *)b;

    for (usize i = 0; i < 64; i++)
    {
        if (ba[i] != 0xAA)
        {
            Dbg_Print("FAIL: buffer A corrupted at [%zu]\n", i);
            Ex_Free(a);
            Ex_Free(b);
            return false;
        }
        if (bb[i] != 0x55)
        {
            Dbg_Print("FAIL: buffer B corrupted at [%zu]\n", i);
            Ex_Free(a);
            Ex_Free(b);
            return false;
        }
    }

    Ex_Free(a);
    Ex_Free(b);
    return true;
}

CI_TEST("pool: each bucket size works", PoolAllBuckets)
{
    static const usize sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

    for (usize s = 0; s < sizeof sizes / sizeof *sizes; s++)
    {
        void *p = Ex_Allocate(sizes[s], EX_TAG('T', 'e', 's', 't'));
        if (!p)
        {
            Dbg_Print("FAIL: Ex_Allocate(%zu) returned null\n", sizes[s]);
            return false;
        }

        u8 *bytes           = (u8 *)p;
        bytes[0]            = 0xDE;
        bytes[sizes[s] - 1] = 0xAD;

        Ex_Free(p);
    }

    return true;
}

CI_TEST("pool: large allocation (> 2048) works", PoolLargeAlloc)
{
    usize size = 8192;
    void *p    = Ex_Allocate(size, EX_TAG('T', 'e', 's', 't'));
    if (!p)
    {
        Dbg_Print("FAIL: Ex_Allocate(%zu) returned null\n", size);
        return false;
    }

    u8 *bytes = (u8 *)p;
    for (usize i = 0; i < size; i++)
    {
        if (bytes[i] != 0)
        {
            Dbg_Print("FAIL: large alloc byte[%zu] = 0x%x\n", i, bytes[i]);
            Ex_Free(p);
            return false;
        }
    }

    Ex_Free(p);
    return true;
}

CI_TEST("pool: free null is safe", PoolFreeNull)
{
    Ex_Free(nullptr);
    return true;
}

#endif /* CI_BUILD */
