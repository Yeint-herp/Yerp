#define DBG_MODULE "x86_64Mm"

#include <arch/CpuCap.h>
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
#define X86_PTE_HUGE     (1ULL << 7)
#define X86_PTE_PAT_4K   (1ULL << 7)
#define X86_PTE_GLOBAL   (1ULL << 8)
#define X86_PTE_PAT_HUGE (1ULL << 12)
#define X86_PTE_NX       (1ULL << 63)

#define X86_PTE_ADDR_MASK    0x000FFFFFFFFFF000ULL
#define X86_PTE_ADDR_MASK_2M 0x000FFFFFFFE00000ULL
#define X86_PTE_ADDR_MASK_1G 0x000FFFFFC0000000ULL

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

static u64 s_TranslateCacheType(Mm_CacheType cacheType, bool huge)
{
    switch (cacheType)
    {
        case kMmCacheWriteBack:
            return 0;

        case kMmCacheWriteThrough:
            return X86_PTE_PWT;

        case kMmCacheUncached:
            return X86_PTE_PCD | X86_PTE_PWT;

        case kMmCacheWriteCombine:
            return huge ? X86_PTE_PAT_HUGE : X86_PTE_PAT_4K;

        default:
            Panic("Mm: invalid cache type %d", cacheType);
    }
}

static void s_IncShareCount(uptr tablePhys)
{
    if (!Mm_IsPfnReady())
        return;

    Mm_Pfn *entry = Mm_GetPfnEntry(tablePhys >> PAGE_SHIFT);
    if (!entry)
        return;

    Core_SpinlockAcquire(&entry->Lock);
    entry->u2.ShareCount++;
    Core_SpinlockRelease(&entry->Lock);
}

static u32 s_DecShareCount(uptr tablePhys)
{
    if (!Mm_IsPfnReady())
        return 1;

    Mm_Pfn *entry = Mm_GetPfnEntry(tablePhys >> PAGE_SHIFT);
    if (!entry)
        return 1;

    Core_SpinlockAcquire(&entry->Lock);
    entry->u2.ShareCount--;
    u32 result = entry->u2.ShareCount;
    Core_SpinlockRelease(&entry->Lock);

    return result;
}

static u64 *s_WalkLevel(u64 *table, uptr tablePhys, usize index, bool allocate)
{
    if (table[index] & X86_PTE_PRESENT)
    {
        if (table[index] & X86_PTE_HUGE)
            return nullptr;

        return s_PhysToTable(table[index]);
    }

    if (!allocate)
        return nullptr;

    void *page;
    uptr  phys;

    if (Mm_IsPfnReady()) [[clang::likely]]
    {
        uptr pfn = Mm_AllocatePages(MM_ALLOC_ZEROED, 1);
        if (pfn == MM_PFN_NULL)
            return nullptr;

        phys = pfn << PAGE_SHIFT;
        page = Mm_PhysToVirt(phys);

        Mm_Pfn *newEntry = Mm_GetPfnEntry(pfn);
        Core_SpinlockAcquire(&newEntry->Lock);
        newEntry->u4.PteFrame   = tablePhys >> PAGE_SHIFT;
        newEntry->u2.ShareCount = 0;
        Core_SpinlockRelease(&newEntry->Lock);

        s_IncShareCount(tablePhys);
    }
    else [[clang::unlikely]]
    {
        page = Mm_PermanentAllocate(PAGE_SIZE, PAGE_SIZE);
        if (!page)
            return nullptr;

        phys = Mm_VirtToPhys(page);
    }

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

    u64 *pdpt = s_WalkLevel(pml4, root, PML4_INDEX(virtualAddr), true);
    if (!pdpt)
        return false;

    const uptr pdptPhys = pml4[PML4_INDEX(virtualAddr)] & X86_PTE_ADDR_MASK;

    u64 *pd = s_WalkLevel(pdpt, pdptPhys, PDPT_INDEX(virtualAddr), true);
    if (!pd)
        return false;

    const uptr pdPhys = pdpt[PDPT_INDEX(virtualAddr)] & X86_PTE_ADDR_MASK;

    u64 *pt = s_WalkLevel(pd, pdPhys, PD_INDEX(virtualAddr), true);
    if (!pt)
        return false;

    const usize index = PT_INDEX(virtualAddr);
    if (pt[index] & X86_PTE_PRESENT)
    {
        Dbg_Print("Mm: attempted to remap already-present VA %p\n", virtualAddr);
        return false;
    }

    pt[index] = (physAddr & X86_PTE_ADDR_MASK) | s_TranslateProt(prot) | s_TranslateCacheType(cacheType, false);

    const uptr ptPhys = pd[PD_INDEX(virtualAddr)] & X86_PTE_ADDR_MASK;
    s_IncShareCount(ptPhys);

    return true;
}

void Arch_MmUnmapPage(uptr root, uptr virtualAddr)
{
    u64 *pml4 = s_PhysToTable(root);

    const usize pml4Idx = PML4_INDEX(virtualAddr);
    if (!(pml4[pml4Idx] & X86_PTE_PRESENT))
        return;
    if (pml4[pml4Idx] & X86_PTE_HUGE)
        return;

    const uptr pdptPhys = pml4[pml4Idx] & X86_PTE_ADDR_MASK;
    u64       *pdpt     = Mm_PhysToVirt(pdptPhys);

    const usize pdptIdx = PDPT_INDEX(virtualAddr);
    if (!(pdpt[pdptIdx] & X86_PTE_PRESENT))
        return;

    if (pdpt[pdptIdx] & X86_PTE_HUGE)
    {
        pdpt[pdptIdx] = 0;

        if (pml4Idx >= PML4_SUPERVISOR_START)
            return;

        if (s_DecShareCount(pdptPhys) != 0)
            return;

        pml4[pml4Idx] = 0;
        Mm_FreePages(pdptPhys >> PAGE_SHIFT, 1);
        return;
    }

    const uptr pdPhys = pdpt[pdptIdx] & X86_PTE_ADDR_MASK;
    u64       *pd     = Mm_PhysToVirt(pdPhys);

    const usize pdIdx = PD_INDEX(virtualAddr);
    if (!(pd[pdIdx] & X86_PTE_PRESENT))
        return;

    if (pd[pdIdx] & X86_PTE_HUGE)
    {
        pd[pdIdx] = 0;

        if (pml4Idx >= PML4_SUPERVISOR_START)
            return;

        if (s_DecShareCount(pdPhys) != 0)
            return;

        pdpt[pdptIdx] = 0;
        Mm_FreePages(pdPhys >> PAGE_SHIFT, 1);

        if (s_DecShareCount(pdptPhys) != 0)
            return;

        pml4[pml4Idx] = 0;
        Mm_FreePages(pdptPhys >> PAGE_SHIFT, 1);
        return;
    }

    const uptr ptPhys = pd[pdIdx] & X86_PTE_ADDR_MASK;
    u64       *pt     = Mm_PhysToVirt(ptPhys);

    const usize ptIdx = PT_INDEX(virtualAddr);
    if (!(pt[ptIdx] & X86_PTE_PRESENT))
    {
        Dbg_Print("Mm: attempted to unmap non-present VA %p\n", virtualAddr);
        return;
    }

    pt[ptIdx] = 0;

    if (pml4Idx >= PML4_SUPERVISOR_START)
        return;

    if (s_DecShareCount(ptPhys) != 0)
        return;

    pd[pdIdx] = 0;
    Mm_FreePages(ptPhys >> PAGE_SHIFT, 1);

    if (s_DecShareCount(pdPhys) != 0)
        return;

    pdpt[pdptIdx] = 0;
    Mm_FreePages(pdPhys >> PAGE_SHIFT, 1);

    if (s_DecShareCount(pdptPhys) != 0)
        return;

    pml4[pml4Idx] = 0;
    Mm_FreePages(pdptPhys >> PAGE_SHIFT, 1);
}

uptr Arch_MmQueryMapping(uptr root, uptr virtualAddr)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 pml4Entry = pml4[PML4_INDEX(virtualAddr)];
    if (!(pml4Entry & X86_PTE_PRESENT))
        return 0;

    u64 *pdpt = s_PhysToTable(pml4Entry);

    u64 pdptEntry = pdpt[PDPT_INDEX(virtualAddr)];
    if (!(pdptEntry & X86_PTE_PRESENT))
        return 0;

    if (pdptEntry & X86_PTE_HUGE)
        return (pdptEntry & X86_PTE_ADDR_MASK_1G) | (virtualAddr & (PAGE_SIZE_1G - 1));

    u64 *pd = s_PhysToTable(pdptEntry);

    u64 pdEntry = pd[PD_INDEX(virtualAddr)];
    if (!(pdEntry & X86_PTE_PRESENT))
        return 0;

    if (pdEntry & X86_PTE_HUGE)
        return (pdEntry & X86_PTE_ADDR_MASK_2M) | (virtualAddr & (PAGE_SIZE_2M - 1));

    u64 *pt = s_PhysToTable(pdEntry);

    u64 entry = pt[PT_INDEX(virtualAddr)];
    if (!(entry & X86_PTE_PRESENT))
        return 0;

    return (entry & X86_PTE_ADDR_MASK) | (virtualAddr & (PAGE_SIZE - 1));
}

static bool s_MapPage1G(uptr root, uptr va, uptr pa, u64 flags, u64 cacheFlags)
{
    u64 *pml4 = s_PhysToTable(root);
    u64 *pdpt = s_WalkLevel(pml4, root, PML4_INDEX(va), true);
    if (!pdpt)
        return false;

    const usize index = PDPT_INDEX(va);
    if (pdpt[index] & X86_PTE_PRESENT)
        return false;

    pdpt[index] = (pa & X86_PTE_ADDR_MASK_1G) | X86_PTE_HUGE | flags | cacheFlags;

    const uptr pdptPhys = pml4[PML4_INDEX(va)] & X86_PTE_ADDR_MASK;
    s_IncShareCount(pdptPhys);

    return true;
}

static bool s_MapPage2M(uptr root, uptr va, uptr pa, u64 flags, u64 cacheFlags)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 *pdpt = s_WalkLevel(pml4, root, PML4_INDEX(va), true);
    if (!pdpt)
        return false;

    const uptr pdptPhys = pml4[PML4_INDEX(va)] & X86_PTE_ADDR_MASK;

    u64 *pd = s_WalkLevel(pdpt, pdptPhys, PDPT_INDEX(va), true);
    if (!pd)
        return false;

    const usize index = PD_INDEX(va);
    if (pd[index] & X86_PTE_PRESENT)
        return false;

    pd[index] = (pa & X86_PTE_ADDR_MASK_2M) | X86_PTE_HUGE | flags | cacheFlags;

    const uptr pdPhys = pdpt[PDPT_INDEX(va)] & X86_PTE_ADDR_MASK;
    s_IncShareCount(pdPhys);

    return true;
}

static bool s_MapPage4K(uptr root, uptr va, uptr pa, u64 flags, u64 cacheFlags)
{
    u64 *pml4 = s_PhysToTable(root);

    u64 *pdpt = s_WalkLevel(pml4, root, PML4_INDEX(va), true);
    if (!pdpt)
        return false;

    const uptr pdptPhys = pml4[PML4_INDEX(va)] & X86_PTE_ADDR_MASK;

    u64 *pd = s_WalkLevel(pdpt, pdptPhys, PDPT_INDEX(va), true);
    if (!pd)
        return false;

    const uptr pdPhys = pdpt[PDPT_INDEX(va)] & X86_PTE_ADDR_MASK;

    u64 *pt = s_WalkLevel(pd, pdPhys, PD_INDEX(va), true);
    if (!pt)
        return false;

    const usize index = PT_INDEX(va);
    if (pt[index] & X86_PTE_PRESENT)
        return false;

    pt[index] = (pa & X86_PTE_ADDR_MASK) | flags | cacheFlags;

    const uptr ptPhys = pd[PD_INDEX(va)] & X86_PTE_ADDR_MASK;
    s_IncShareCount(ptPhys);

    return true;
}

usize Arch_MmMapRegion(uptr root, uptr virtualAddr, uptr physAddr, usize size, u32 prot, Mm_CacheType cacheType)
{
    ASSERT(IsAligned(virtualAddr, PAGE_SIZE));
    ASSERT(IsAligned(physAddr, PAGE_SIZE));
    ASSERT(IsAligned(size, PAGE_SIZE));

    const bool has1G = Arch_CpuHasCap(CPUCAP_1GB_PAGES);

    const u64 protBits      = s_TranslateProt(prot);
    const u64 cacheBits4K   = s_TranslateCacheType(cacheType, false);
    const u64 cacheBitsHuge = s_TranslateCacheType(cacheType, true);

    uptr  va       = virtualAddr;
    uptr  pa       = physAddr;
    uptr  end      = virtualAddr + size;
    usize mappings = 0;

    while (va < end)
    {
        const usize remaining = end - va;

        if (has1G && remaining >= PAGE_SIZE_1G && IsAligned(va, PAGE_SIZE_1G) && IsAligned(pa, PAGE_SIZE_1G))
        {
            if (!s_MapPage1G(root, va, pa, protBits, cacheBitsHuge))
                goto fail;

            va += PAGE_SIZE_1G;
            pa += PAGE_SIZE_1G;
            mappings++;
            continue;
        }

        if (remaining >= PAGE_SIZE_2M && IsAligned(va, PAGE_SIZE_2M) && IsAligned(pa, PAGE_SIZE_2M))
        {
            if (!s_MapPage2M(root, va, pa, protBits, cacheBitsHuge))
                goto fail;

            va += PAGE_SIZE_2M;
            pa += PAGE_SIZE_2M;
            mappings++;
            continue;
        }

        if (!s_MapPage4K(root, va, pa, protBits, cacheBits4K))
            goto fail;

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        mappings++;
    }

    return mappings;

fail:
    Log(ERROR, "Arch_MmMapRegion: failed at VA %p (mapped %zu so far)", va, mappings);
    return 0;
}

uptr Arch_MmCreateUserPageTable(void)
{
    const uptr pfn = Mm_AllocatePages(MM_ALLOC_ZEROED, 1);
    if (pfn == MM_PFN_NULL)
        return 0;

    const uptr phys    = pfn << PAGE_SHIFT;
    u64       *newPml4 = Mm_PhysToVirt(phys);

    u64 *currentPml4 = s_PhysToTable(Arch_MmGetCurrentRoot());
    for (usize i = PML4_SUPERVISOR_START; i < 512; i++)
        newPml4[i] = currentPml4[i];

    return phys;
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

            if (pdpt[j] & X86_PTE_HUGE)
                continue;

            u64 *pd = s_PhysToTable(pdpt[j]);
            for (usize k = 0; k < 512; k++)
            {
                if (!(pd[k] & X86_PTE_PRESENT))
                    continue;

                if (pd[k] & X86_PTE_HUGE)
                    continue;

                Mm_FreePages((pd[k] & X86_PTE_ADDR_MASK) >> PAGE_SHIFT, 1);
            }

            Mm_FreePages((pdpt[j] & X86_PTE_ADDR_MASK) >> PAGE_SHIFT, 1);
        }

        Mm_FreePages((pml4[i] & X86_PTE_ADDR_MASK) >> PAGE_SHIFT, 1);
    }

    Mm_FreePages(root >> PAGE_SHIFT, 1);
}
