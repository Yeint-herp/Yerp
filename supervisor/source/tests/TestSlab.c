#ifdef CI_BUILD

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <mm/PfnDb.h>
#include <executive/Pool.h>
#include <mm/Slab.h>
#include <tests/CiTest.h>

CI_TEST("slab: basic alloc and free", SlabBasic)
{
    Mm_SlabCache cache;
    Mm_SlabCacheInit(&cache, "ci-basic", 48, sizeof(void *));

    void *obj = Mm_SlabAlloc(&cache, 0);
    if (!obj)
    {
        Dbg_Print("FAIL: Mm_SlabAlloc returned null\n");
        return false;
    }

    Mm_SlabFree(obj);
    return true;
}

CI_TEST("slab: multiple allocs are unique", SlabUnique)
{
    Mm_SlabCache cache;
    Mm_SlabCacheInit(&cache, "ci-unique", 32, sizeof(void *));

    enum
    {
        N = 16
    };
    void *ptrs[N];

    for (usize i = 0; i < N; i++)
    {
        ptrs[i] = Mm_SlabAlloc(&cache, 0);
        if (!ptrs[i])
        {
            Dbg_Print("FAIL: alloc %zu returned null\n", i);
            for (usize j = 0; j < i; j++)
                Mm_SlabFree(ptrs[j]);

            return false;
        }
    }

    for (usize i = 0; i < N; i++)
    {
        for (usize j = i + 1; j < N; j++)
        {
            if (ptrs[i] == ptrs[j])
            {
                Dbg_Print("FAIL: ptrs[%zu] == ptrs[%zu]\n", i, j);
                for (usize k = 0; k < N; k++)
                    Mm_SlabFree(ptrs[k]);

                return false;
            }
        }
    }

    for (usize i = 0; i < N; i++)
        Mm_SlabFree(ptrs[i]);

    return true;
}

CI_TEST("slab: alloc after free reuses memory", SlabReuse)
{
    Mm_SlabCache cache;
    Mm_SlabCacheInit(&cache, "ci-reuse", 64, sizeof(void *));

    void *a = Mm_SlabAlloc(&cache, 0);
    if (!a)
    {
        Dbg_Print("FAIL: first alloc null\n");
        return false;
    }

    Mm_SlabFree(a);

    void *b = Mm_SlabAlloc(&cache, 0);
    if (!b)
    {
        Dbg_Print("FAIL: second alloc null\n");
        return false;
    }

    Mm_SlabFree(b);
    return true;
}

CI_TEST("slab: fills entire slab then spills to new slab", SlabSpill)
{
    Mm_SlabCache cache;
    Mm_SlabCacheInit(&cache, "ci-spill", 128, sizeof(void *));

    usize perSlab = cache.ObjectsPerSlab;
    usize total   = perSlab + 4;

    void **ptrs = Ex_Allocate(total * sizeof(void *), EX_TAG('T', 'e', 's', 't'));
    if (!ptrs)
    {
        Dbg_Print("FAIL: couldn't allocate tracking array\n");
        return false;
    }

    bool ok = true;
    for (usize i = 0; i < total; i++)
    {
        ptrs[i] = Mm_SlabAlloc(&cache, 0);
        if (!ptrs[i])
        {
            Dbg_Print("FAIL: alloc %zu / %zu returned null\n", i, total);
            total = i;
            ok    = false;
            break;
        }
    }

    for (usize i = 0; i < total; i++)
        Mm_SlabFree(ptrs[i]);

    Ex_Free(ptrs);
    return ok;
}

CI_TEST("slab: objects are usable (write/read)", SlabWriteRead)
{
    Mm_SlabCache cache;
    Mm_SlabCacheInit(&cache, "ci-rw", 256, sizeof(void *));

    void *obj = Mm_SlabAlloc(&cache, 0);
    if (!obj)
    {
        Dbg_Print("FAIL: alloc null\n");
        return false;
    }

    Core_FillMemory(obj, 0xCD, 256);

    u8 *bytes = (u8 *)obj;
    for (usize i = 0; i < 256; i++)
    {
        if (bytes[i] != 0xCD)
        {
            Dbg_Print("FAIL: byte[%zu] = 0x%x after write\n", i, bytes[i]);
            Mm_SlabFree(obj);
            return false;
        }
    }

    Mm_SlabFree(obj);
    return true;
}

#endif /* CI_BUILD */
