#define DBG_MODULE "Vas"

#include <arch/MmArch.h>
#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/Layout.h>
#include <mm/MemMap.h>
#include <mm/PfnDb.h>
#include <executive/Pool.h>
#include <mm/Vas.h>

static Mm_AddressSpace s_SupervisorVas;

extern char __start[], __end[];
extern char __text_start[], __text_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __data_end[];
extern char __percpu_start[], __percpu_end[];
extern char __bss_start[], __bss_end[];

static void s_RemapSupervisorImage(uptr newRoot)
{
    uptr currentRoot = Arch_MmGetCurrentRoot();

    struct
    {
        uptr va;
        uptr size;
        u32  prot;
    } sections[] = {
        {(uptr)__text_start, (uptr)__text_end - (uptr)__text_start, MM_PROT_READ | MM_PROT_EXECUTE},
        {(uptr)__rodata_start, (uptr)__rodata_end - (uptr)__rodata_start, MM_PROT_READ},
        {(uptr)__data_start, (uptr)__data_end - (uptr)__data_start, MM_PROT_READ | MM_PROT_WRITE},
        {(uptr)__percpu_start, (uptr)__percpu_end - (uptr)__percpu_start, MM_PROT_READ},
        {(uptr)__bss_start, (uptr)__bss_end - (uptr)__bss_start, MM_PROT_READ | MM_PROT_WRITE},
    };

    usize totalPages = 0;

    for (usize i = 0; i < sizeof sections / sizeof *sections; i++)
    {
        uptr start = AlignDown(sections[i].va, PAGE_SIZE);
        uptr end   = AlignUp(sections[i].va + sections[i].size, PAGE_SIZE);

        for (uptr va = start; va < end; va += PAGE_SIZE)
        {
            uptr pa = Arch_MmQueryMapping(currentRoot, va);
            if (pa == 0)
                Panic("supervisor section VA %p not mapped in bootloader tables", va);

            pa &= ~(uptr)(PAGE_SIZE - 1);

            bool ok = Arch_MmMapPage(newRoot, va, pa, sections[i].prot | MM_PROT_GLOBAL, MM_CACHE_WRITEBACK);
            if (!ok)
                Panic("failed to remap supervisor VA %p", va);

            totalPages++;
        }
    }

    /// gap-fill all of the supervisor pages that don't fall into a named section above as RW.
    uptr imgStart = AlignDown((uptr)__start, PAGE_SIZE);
    uptr imgEnd   = AlignUp((uptr)__end, PAGE_SIZE);

    for (uptr va = imgStart; va < imgEnd; va += PAGE_SIZE)
    {
        if (Arch_MmQueryMapping(newRoot, va) != 0)
            continue;

        uptr pa = Arch_MmQueryMapping(currentRoot, va);
        if (pa == 0)
            continue;

        pa &= ~(uptr)(PAGE_SIZE - 1);

        bool ok = Arch_MmMapPage(newRoot, va, pa, MM_PROT_READ | MM_PROT_WRITE | MM_PROT_GLOBAL, MM_CACHE_WRITEBACK);
        if (!ok)
            Panic("failed to gap-fill supervisor VA %p", va);

        totalPages++;
    }

    Log(INFO, "remapped supervisor image: %zu pages (%zZ)", totalPages, totalPages * PAGE_SIZE);
}

static void s_RemapHhdm(uptr newRoot)
{
    const Mm_VaLayout         *layout = Mm_GetVaLayout();
    const Mm_SupervisorMemMap *memMap = Mm_GetSupervisorMemMap();

    usize totalMappings = 0;
    for (usize i = 0; i < memMap->Count; i++)
    {
        const Mm_MemRegion *region = &memMap->Regions[i];

        uptr physStart = AlignDown(region->Base, PAGE_SIZE);
        uptr physEnd   = AlignUp(region->Base + region->Length, PAGE_SIZE);

        while (physStart < physEnd)
        {
            const uptr va = layout->HhdmBase + physStart;

            if (Arch_MmQueryMapping(newRoot, va) != 0)
            {
                physStart += PAGE_SIZE;
                continue;
            }

            const uptr runStart = physStart;

            while (physStart < physEnd && Arch_MmQueryMapping(newRoot, layout->HhdmBase + physStart) == 0)
                physStart += PAGE_SIZE;

            const uptr runSize = physStart - runStart;

            usize mapped = Arch_MmMapRegion(newRoot, layout->HhdmBase + runStart, runStart, runSize,
                                            MM_PROT_READ | MM_PROT_WRITE | MM_PROT_GLOBAL, MM_CACHE_WRITEBACK);

            if (mapped == 0)
                Panic("failed to map HHDM region PA %p, size %zu", runStart, runSize);

            totalMappings += mapped;
        }
    }

    Log(INFO, "remapped HHDM: %zu mappings", totalMappings);
}

void Mm_SupervisorVasInit(void)
{
    const Mm_VaLayout *layout = Mm_GetVaLayout();

    void *pml4 = Mm_PermanentAllocate(PAGE_SIZE, PAGE_SIZE);
    if (!pml4)
        Panic("failed to allocate supervisor PML4");

    uptr newRoot = Mm_VirtToPhys(pml4);
    Log(INFO, "building supervisor page tables (root = %p)", newRoot);

    s_RemapSupervisorImage(newRoot);
    s_RemapHhdm(newRoot);
    Mm_PfnDbMapInto(newRoot);

    Arch_MmSwitchRoot(newRoot);
    Arch_MmFlushTlbGlobal();
    Log(INFO, "switched to supervisor page tables");

    Mm_VasInit(&s_SupervisorVas, layout->DynamicSpaceBase, layout->DynamicSpaceBase + layout->DynamicSpaceSize,
               newRoot);

    Log(INFO, "supervisor VAS: dynamic space %p .. %p (%zZ)", layout->DynamicSpaceBase,
        layout->DynamicSpaceBase + layout->DynamicSpaceSize, layout->DynamicSpaceSize);
}

Mm_AddressSpace *Mm_GetSupervisorVas(void)
{
    return &s_SupervisorVas;
}

void s_VadUnmapAndRelease(Mm_AddressSpace *vas, Mm_Vad *vad)
{
    const uptr  root      = vas->PageTableRoot;
    const usize pageCount = vad->RegionSize >> PAGE_SHIFT;

    for (usize i = 0; i < pageCount; i++)
    {
        const uptr va = vad->BaseAddress + (i << PAGE_SHIFT);

        if (vad->Type == VAD_TYPE_MMIO)
            Arch_MmUnmapPage(root, va);
        else if (vad->Type == VAD_TYPE_PRIVATE)
        {
            const uptr pa = Arch_MmQueryMapping(root, va);

            Arch_MmUnmapPage(root, va);
            if (pa != MM_PFN_NULL)
                Mm_FreePages(pa >> PAGE_SHIFT, 1);
        }
        else
            Arch_MmUnmapPage(root, va);
    }

    Arch_MmInvalidateRange(vad->BaseAddress, pageCount);
}

void Mm_VasInit(Mm_AddressSpace *vas, uptr lowest, uptr highest, uptr root)
{
    Dsa_AvlTreeInit(&vas->VadTree);
    Core_SpinlockInit(&vas->Lock);

    vas->LowestAddress  = lowest;
    vas->HighestAddress = highest;
    vas->PageTableRoot  = root;

    Arch_AtomicStore32(&vas->ReferenceCount, 1);
}

void Mm_VasDestroy(Mm_AddressSpace *vas)
{
    Core_SpinlockAcquire(&vas->Lock);

    Dsa_AvlNode *node = Dsa_AvlFirst(&vas->VadTree);
    while (node)
    {
        Mm_Vad      *vad  = Dsa_AvlEntry(node, Mm_Vad, TreeNode);
        Dsa_AvlNode *next = Dsa_AvlNext(node);

        s_VadUnmapAndRelease(vas, vad);

        Dsa_AvlRemove(&vas->VadTree, vad->BaseAddress, Mm_VadBaseCmp);
        Mm_VadDestroy(vad);

        node = next;
    }

    Core_SpinlockRelease(&vas->Lock);

    if (vas != &s_SupervisorVas)
        Arch_MmDestroyUserPageTable(vas->PageTableRoot);
}

static uptr s_VasFindFreeRegion(Mm_AddressSpace *vas, uptr hint, usize size)
{
    uptr cursor = vas->LowestAddress;

    if (hint >= vas->LowestAddress && hint + size <= vas->HighestAddress)
    {
        Dsa_AvlNode *overlap = Dsa_AvlFind(&vas->VadTree, hint, Mm_VadContainsCmp);
        if (!overlap)
        {
            bool clear = true;
            Dsa_AvlForEach(&vas->VadTree, iter)
            {
                Mm_Vad *v = Dsa_AvlEntry(iter, Mm_Vad, TreeNode);
                if (v->BaseAddress >= hint + size)
                    break;

                if (v->BaseAddress + v->RegionSize > hint)
                {
                    clear = false;
                    break;
                }
            }
            if (clear)
                return hint;
        }
    }

    Dsa_AvlForEach(&vas->VadTree, iter)
    {
        Mm_Vad *vad = Dsa_AvlEntry(iter, Mm_Vad, TreeNode);

        if (vad->BaseAddress >= cursor + size)
            return cursor;

        uptr vadEnd = vad->BaseAddress + vad->RegionSize;
        if (vadEnd > cursor)
            cursor = vadEnd;
    }

    if (cursor + size <= vas->HighestAddress)
        return cursor;

    return 0;
}

uptr Mm_VasAllocateRegion(Mm_AddressSpace *vas, uptr hint, usize size, Mm_VadType type, u32 prot, u32 flags)
{
    size = AlignUp(size, PAGE_SIZE);
    if (hint)
        hint = AlignDown(hint, PAGE_SIZE);

    Core_SpinlockAcquire(&vas->Lock);

    uptr base = s_VasFindFreeRegion(vas, hint, size);
    if (base == 0)
    {
        Core_SpinlockRelease(&vas->Lock);
        Log(WARN, "VasAllocateRegion: no free region of size %zu", size);
        return 0;
    }

    Mm_Vad *vad = Mm_VadCreate(base, size, type, prot, MM_CACHE_WRITEBACK, flags);
    if (!vad)
    {
        Core_SpinlockRelease(&vas->Lock);
        return 0;
    }

    bool ok = Dsa_AvlInsert(&vas->VadTree, &vad->TreeNode, Mm_VadInsertCmp);
    ASSERT(ok);

    if (flags & VAD_FLAG_COMMITTED)
    {
        usize pageCount = size >> PAGE_SHIFT;
        for (usize i = 0; i < pageCount; i++)
        {
            uptr pa = Mm_AllocatePages(flags & VAD_FLAG_ZEROED ? MM_ALLOC_ZEROED : MM_ALLOC_ANY, 1);
            if (pa == MM_PFN_NULL)
            {
                for (usize j = 0; j < i; j++)
                {
                    uptr va    = base + (j << PAGE_SHIFT);
                    uptr mapPa = Arch_MmQueryMapping(vas->PageTableRoot, va);

                    Arch_MmUnmapPage(vas->PageTableRoot, va);
                    if (mapPa != MM_PFN_NULL)
                        Mm_FreePages(mapPa >> PAGE_SHIFT, 1);
                }
                Arch_MmInvalidateRange(base, i);

                Dsa_AvlRemove(&vas->VadTree, base, Mm_VadBaseCmp);
                Mm_VadDestroy(vad);

                Core_SpinlockRelease(&vas->Lock);
                Log(ERROR, "VasAllocateRegion: physical page allocation failed");
                return 0;
            }

            const uptr phys = pa << PAGE_SHIFT;
            const uptr va   = base + (i << PAGE_SHIFT);
            if (!Arch_MmMapPage(vas->PageTableRoot, va, phys, prot, MM_CACHE_WRITEBACK))
            {
                Mm_FreePages(pa, 1);

                for (usize j = 0; j < i; j++)
                {
                    uptr prevVa = base + (j << PAGE_SHIFT);
                    uptr mapPa  = Arch_MmQueryMapping(vas->PageTableRoot, prevVa);

                    Arch_MmUnmapPage(vas->PageTableRoot, prevVa);
                    if (mapPa != MM_PFN_NULL)
                        Mm_FreePages(mapPa >> PAGE_SHIFT, 1);
                }
                Arch_MmInvalidateRange(base, i);

                Dsa_AvlRemove(&vas->VadTree, base, Mm_VadBaseCmp);
                Mm_VadDestroy(vad);

                Core_SpinlockRelease(&vas->Lock);
                Log(ERROR, "VasAllocateRegion: page table mapping failed");
                return 0;
            }
        }
    }

    Core_SpinlockRelease(&vas->Lock);
    return base;
}

bool Mm_VasFreeRegion(Mm_AddressSpace *vas, uptr baseAddress)
{
    Core_SpinlockAcquire(&vas->Lock);

    Dsa_AvlNode *node = Dsa_AvlFind(&vas->VadTree, baseAddress, Mm_VadBaseCmp);
    if (!node)
    {
        Core_SpinlockRelease(&vas->Lock);
        Log(WARN, "VasFreeRegion: no VAD at %p", baseAddress);
        return false;
    }

    Mm_Vad *vad = Dsa_AvlEntry(node, Mm_Vad, TreeNode);

    s_VadUnmapAndRelease(vas, vad);

    Dsa_AvlRemove(&vas->VadTree, baseAddress, Mm_VadBaseCmp);
    Mm_VadDestroy(vad);

    Core_SpinlockRelease(&vas->Lock);
    return true;
}

bool Mm_VasQueryVad(Mm_AddressSpace *vas, uptr address, Mm_VadInfo *out)
{
    Core_SpinlockAcquire(&vas->Lock);

    Dsa_AvlNode *node = Dsa_AvlFind(&vas->VadTree, address, Mm_VadContainsCmp);
    if (!node)
    {
        Core_SpinlockRelease(&vas->Lock);
        return false;
    }

    Mm_Vad *vad = Dsa_AvlEntry(node, Mm_Vad, TreeNode);

    out->BaseAddress = vad->BaseAddress;
    out->RegionSize  = vad->RegionSize;
    out->Type        = vad->Type;
    out->Protection  = vad->Protection;
    out->CacheType   = vad->CacheType;
    out->Flags       = vad->Flags;

    Core_SpinlockRelease(&vas->Lock);
    return true;
}

uptr Mm_MapIoSpace(uptr physBase, usize size, Mm_CacheType cacheType)
{
    Mm_AddressSpace *vas = Mm_GetSupervisorVas();

    uptr  offset    = physBase & (PAGE_SIZE - 1);
    uptr  alignedPa = physBase & ~(PAGE_SIZE - 1);
    usize mapSize   = (size + offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    Core_SpinlockAcquire(&vas->Lock);

    uptr base = s_VasFindFreeRegion(vas, 0, mapSize);
    if (base == 0)
    {
        Core_SpinlockRelease(&vas->Lock);
        Log(ERROR, "MapIoSpace: no free dynamic space for %zu bytes", mapSize);
        return 0;
    }

    Mm_Vad *vad =
        Mm_VadCreate(base, mapSize, VAD_TYPE_MMIO, MM_PROT_READ | MM_PROT_WRITE, cacheType, VAD_FLAG_COMMITTED);
    if (!vad)
    {
        Core_SpinlockRelease(&vas->Lock);
        return 0;
    }

    vad->PhysicalBase = alignedPa;

    bool inserted = Dsa_AvlInsert(&vas->VadTree, &vad->TreeNode, Mm_VadInsertCmp);
    ASSERT(inserted);

    usize pageCount = mapSize >> PAGE_SHIFT;
    for (usize i = 0; i < pageCount; i++)
    {
        uptr va = base + (i << PAGE_SHIFT);
        uptr pa = alignedPa + (i << PAGE_SHIFT);

        if (!Arch_MmMapPage(vas->PageTableRoot, va, pa, MM_PROT_READ | MM_PROT_WRITE | MM_PROT_GLOBAL, cacheType))
        {
            for (usize j = 0; j < i; j++)
                Arch_MmUnmapPage(vas->PageTableRoot, base + (j << PAGE_SHIFT));
            Arch_MmInvalidateRange(base, i);

            Dsa_AvlRemove(&vas->VadTree, base, Mm_VadBaseCmp);
            Mm_VadDestroy(vad);

            Core_SpinlockRelease(&vas->Lock);
            Log(ERROR, "MapIoSpace: page table allocation failed");
            return 0;
        }
    }

    Core_SpinlockRelease(&vas->Lock);

    Log(DEBUG, "MapIoSpace: %p -> %p (%zu pages, cache = %d)", alignedPa, base, pageCount, cacheType);

    return base + offset;
}

void Mm_UnmapIoSpace(uptr virtualAddr)
{
    uptr aligned = AlignDown(virtualAddr, PAGE_SIZE);

    Mm_AddressSpace *vas = Mm_GetSupervisorVas();

    Core_SpinlockAcquire(&vas->Lock);

    Dsa_AvlNode *node = Dsa_AvlFind(&vas->VadTree, aligned, Mm_VadContainsCmp);
    if (!node)
    {
        Core_SpinlockRelease(&vas->Lock);
        Log(WARN, "UnmapIoSpace: no VAD for %p", virtualAddr);
        return;
    }

    Mm_Vad *vad = Dsa_AvlEntry(node, Mm_Vad, TreeNode);
    ASSERT(vad->Type == VAD_TYPE_MMIO);

    uptr  base      = vad->BaseAddress;
    usize pageCount = vad->RegionSize >> PAGE_SHIFT;

    Dsa_AvlRemove(&vas->VadTree, base, Mm_VadBaseCmp);
    Core_SpinlockRelease(&vas->Lock);

    for (usize i = 0; i < pageCount; i++)
        Arch_MmUnmapPage(vas->PageTableRoot, base + (i << PAGE_SHIFT));

    Arch_MmInvalidateRange(base, pageCount);
    Mm_VadDestroy(vad);
}

void Mm_VasReference(Mm_AddressSpace *vas)
{
    Arch_AtomicAdd32(&vas->ReferenceCount, 1);
}

bool Mm_VasDereference(Mm_AddressSpace *vas)
{
    i32 old = Arch_AtomicSub32(&vas->ReferenceCount, 1);
    if (old == 1)
    {
        Mm_VasDestroy(vas);
        return true;
    }

    return false;
}
