#define DBG_MODULE "MmVad"

#include <debug/Panic.h>
#include <mm/PfnDb.h>
#include <executive/Pool.h>
#include <mm/Vad.h>

i32 Mm_VadInsertCmp(Dsa_AvlNode *a, Dsa_AvlNode *b)
{
    Mm_Vad *va = Dsa_AvlEntry(a, Mm_Vad, TreeNode);
    Mm_Vad *vb = Dsa_AvlEntry(b, Mm_Vad, TreeNode);

    if (va->BaseAddress < vb->BaseAddress)
        return -1;

    if (va->BaseAddress > vb->BaseAddress)
        return 1;

    return 0;
}

i32 Mm_VadContainsCmp(void *key, Dsa_AvlNode *node)
{
    const uptr addr = (uptr)key;
    Mm_Vad    *vad  = Dsa_AvlEntry(node, Mm_Vad, TreeNode);

    if (addr < vad->BaseAddress)
        return -1;

    if (addr >= vad->BaseAddress + vad->RegionSize)
        return 1;

    return 0;
}

i32 Mm_VadBaseCmp(void *key, Dsa_AvlNode *node)
{
    const uptr addr = (uptr)key;
    Mm_Vad    *vad  = Dsa_AvlEntry(node, Mm_Vad, TreeNode);

    if (addr < vad->BaseAddress)
        return -1;

    if (addr > vad->BaseAddress)
        return 1;

    return 0;
}

Mm_Vad *Mm_VadCreate(uptr base, usize size, Mm_VadType type, u32 prot, Mm_CacheType cache, u32 flags)
{
    ASSERT(IsAligned(base, PAGE_SIZE));
    ASSERT(IsAligned(size, PAGE_SIZE));
    ASSERT(size > 0);

    Mm_Vad *vad = Ex_Allocate(sizeof *vad, EX_TAG_VAD);
    if (!vad)
        return nullptr;

    vad->TreeNode.Left    = nullptr;
    vad->TreeNode.Right   = nullptr;
    vad->TreeNode.Parent  = nullptr;
    vad->TreeNode.Balance = 0;

    vad->BaseAddress  = base;
    vad->RegionSize   = size;
    vad->Type         = type;
    vad->Protection   = prot;
    vad->CacheType    = cache;
    vad->Flags        = flags;
    vad->PhysicalBase = MM_PFN_NULL;

    return vad;
}

void Mm_VadDestroy(Mm_Vad *vad)
{
    Ex_Free(vad);
}
