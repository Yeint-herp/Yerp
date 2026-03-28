#define DBG_MODULE "x86_64Mm"

#include <arch/MmArch.h>
#include <arch/x86_64/Msr.h>
#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/Layout.h>
#include <mm/PfnDb.h>

static Mm_VaLayout s_Layout = {};

extern char __start[], __end[];

void Arch_MmInit(void)
{
    s_Layout.HhdmBase = Mm_GetHhdmBase();
    s_Layout.HhdmSize = 0x0000400000000000ULL;

    s_Layout.PfnDbBase = 0xFFFFC00000000000ULL;
    s_Layout.PfnDbSize = 0x0000200000000000ULL;

    s_Layout.DynamicSpaceBase = 0xFFFFF00000000000ULL;
    s_Layout.DynamicSpaceSize = 0x00000F0000000000ULL;

    s_Layout.ModuleSpaceBase = AlignUp((uptr)__end, PAGE_SIZE);
    s_Layout.ModuleSpaceSize = 0xFFFFFFFFFFFFFFFF - s_Layout.ModuleSpaceBase;

    s_Layout.SupervisorImageBase = (uptr)__start;
    s_Layout.SupervisorImageSize = __end - __start;

    /// set PAT4 to WC.
    u64 pat = X86_64_ReadMsr(0x277);

    pat &= ~(0x7ULL << 32);
    pat |= (0x1ULL << 32);

    X86_64_WriteMsr(0x277, pat);
}

const Mm_VaLayout *Mm_GetVaLayout(void)
{
    return &s_Layout;
}

#define X86_PTE_PRESENT  (1ULL << 0)
#define X86_PTE_WRITABLE (1ULL << 1)
#define X86_PTE_USER     (1ULL << 2)
#define X86_PTE_PWT      (1ULL << 3)
#define X86_PTE_PCD      (1ULL << 4)
#define X86_PTE_PAT      (1ULL << 7)
#define X86_PTE_GLOBAL   (1ULL << 8)
#define X86_PTE_NX       (1ULL << 63)

#define X86_PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

#define PML4_SUPERVISOR_START 256

static u64 *s_PhysToTable(uptr phys)
{
    return Mm_PhysToVirt(phys & X86_PTE_ADDR_MASK);
}

static u64 s_TranslateProt(u32 prot)
{
    u64 pte = X86_PTE_PRESENT;

    if (prot & MM_PROT_WRITE)
        pte |= X86_PTE_WRITABLE;
    if (!(prot & MM_PROT_EXECUTE))
        pte |= X86_PTE_NX;
    if (prot & MM_PROT_USER)
        pte |= X86_PTE_USER;
    if (prot & MM_PROT_GLOBAL)
        pte |= X86_PTE_GLOBAL;

    return pte;
}

static u64 s_TranslateCacheType(Mm_CacheType cacheType)
{
    switch (cacheType)
    {
        case MM_CACHE_WRITEBACK:
            return 0;

        case MM_CACHE_WRITE_THROUGH:
            return X86_PTE_PWT;

        case MM_CACHE_UNCACHED:
            return X86_PTE_PCD | X86_PTE_PWT;

        case MM_CACHE_WRITE_COMBINE:
            return X86_PTE_PAT;

        default:
            Panic("Mm: invalid cache type %d", cacheType);
    }
}

static u64 *s_WalkLevel(u64 *table, usize index, bool allocate)
{
    if (table[index] & X86_PTE_PRESENT)
        return s_PhysToTable(table[index]);

    if (!allocate)
        return nullptr;

    void *page = Mm_EarlyAllocate(PAGE_SIZE, PAGE_SIZE);
    if (!page)
        return nullptr;

    uptr phys = Mm_VirtToPhys(page);

    table[index] = phys | X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_USER;

    return page;
}

uptr Arch_MmGetCurrentRoot(void)
{
    u64 cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    return cr3 & X86_PTE_ADDR_MASK;
}

void Arch_MmSwitchRoot(uptr root)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(root) : "memory");
}

void Arch_MmInvalidatePage(uptr virtualAddr)
{
    __asm__ volatile("invlpg (%0)" ::"r"(virtualAddr) : "memory");
}

void Arch_MmFlushTlbGlobal(void)
{
    u64 cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %0, %%cr4" ::"r"(cr4 & ~(1ULL << 7)) : "memory");
    __asm__ volatile("mov %0, %%cr4" ::"r"(cr4) : "memory");
}

void Arch_MmInvalidateRange(uptr virtualAddr, usize pageCount)
{
    for (usize i = 0; i < pageCount; i++)
        Arch_MmInvalidatePage(virtualAddr + i * PAGE_SIZE);
}

bool Arch_MmMapPage(uptr root, uptr virtualAddr, uptr physAddr, u32 prot, Mm_CacheType cacheType)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 *pdpt = s_WalkLevel(pml4, PML4_INDEX(virtualAddr), true);
    if (!pdpt)
        return false;

    u64 *pd = s_WalkLevel(pdpt, PDPT_INDEX(virtualAddr), true);
    if (!pd)
        return false;

    u64 *pt = s_WalkLevel(pd, PD_INDEX(virtualAddr), true);
    if (!pt)
        return false;

    const usize index = PT_INDEX(virtualAddr);
    if (pt[index] & X86_PTE_PRESENT)
    {
        Dbg_Print("Mm: attempted to remap already-present VA %p\n", virtualAddr);
        return false;
    }

    pt[index] = (physAddr & X86_PTE_ADDR_MASK) | s_TranslateProt(prot) | s_TranslateCacheType(cacheType);

    return true;
}

void Arch_MmUnmapPage(uptr root, uptr virtualAddr)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 *pdpt = s_WalkLevel(pml4, PML4_INDEX(virtualAddr), false);
    if (!pdpt)
        return;

    u64 *pd = s_WalkLevel(pdpt, PDPT_INDEX(virtualAddr), false);
    if (!pd)
        return;

    u64 *pt = s_WalkLevel(pd, PD_INDEX(virtualAddr), false);
    if (!pt)
        return;

    const usize index = PT_INDEX(virtualAddr);
    if (!(pt[index] & X86_PTE_PRESENT))
    {
        Dbg_Print("Mm: attempted to unmap non-present VA %p\n", virtualAddr);
        return;
    }

    pt[index] = 0;
}

uptr Arch_MmQueryMapping(uptr root, uptr virtualAddr)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 *pdpt = s_WalkLevel(pml4, PML4_INDEX(virtualAddr), false);
    if (!pdpt)
        return 0;

    u64 *pd = s_WalkLevel(pdpt, PDPT_INDEX(virtualAddr), false);
    if (!pd)
        return 0;

    u64 *pt = s_WalkLevel(pd, PD_INDEX(virtualAddr), false);
    if (!pt)
        return 0;

    const u64 entry = pt[PT_INDEX(virtualAddr)];
    if (!(entry & X86_PTE_PRESENT))
        return 0;

    return (entry & X86_PTE_ADDR_MASK) | (virtualAddr & (PAGE_SIZE - 1));
}

uptr Arch_MmCreateUserPageTable(void)
{
    u64 *newPml4 = Mm_EarlyAllocate(PAGE_SIZE, PAGE_SIZE);
    if (!newPml4)
        return 0;

    for (usize i = 0; i < PML4_SUPERVISOR_START; i++)
        newPml4[i] = 0;

    u64 *currentPml4 = s_PhysToTable(Arch_MmGetCurrentRoot());
    for (usize i = PML4_SUPERVISOR_START; i < 512; i++)
        newPml4[i] = currentPml4[i];

    return Mm_VirtToPhys(newPml4);
}

void Arch_MmDestroyUserPageTable(uptr root)
{
    ASSERT(root != Arch_MmGetCurrentRoot());

    u64 *pml4 = s_PhysToTable(root);
    for (usize i = 0; i < PML4_SUPERVISOR_START; i++)
    {
        if (!(pml4[i] & X86_PTE_PRESENT))
            continue;

        u64 *pdpt = s_PhysToTable(pml4[i]);
        for (usize j = 0; j < 512; j++)
        {
            if (!(pdpt[j] & X86_PTE_PRESENT))
                continue;

            u64 *pd = s_PhysToTable(pdpt[j]);
            for (usize k = 0; k < 512; k++)
            {
                if (!(pd[k] & X86_PTE_PRESENT))
                    continue;

                Mm_FreePages(pd[k] & X86_PTE_ADDR_MASK, 1);
            }

            Mm_FreePages(pdpt[j] & X86_PTE_ADDR_MASK, 1);
        }

        Mm_FreePages(pml4[i] & X86_PTE_ADDR_MASK, 1);
    }

    Mm_FreePages(root, 1);
}
