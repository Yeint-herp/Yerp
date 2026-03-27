#define DBG_MODULE "x86_64MmEarly"

#include <core/Memory.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <mm/Early.h>
#include <mm/Layout.h>
#include <mm/PfnDb.h>

static Mm_VaLayout s_Layout = {};

extern char __start[], __end[];

void Arch_MmLayoutInit()
{
    s_Layout.HhdmBase = Mm_GetHhdmBase();
    s_Layout.HhdmSize = 0x0000400000000000ULL;

    s_Layout.PfnDbBase = 0xFFFFC00000000000ULL;
    s_Layout.PfnDbSize = 0x0000200000000000ULL;

    s_Layout.DynamicSpaceBase = 0xFFFFF00000000000ULL;
    s_Layout.DynamicSpaceSize = 0x00000F0000000000ULL;

    s_Layout.ModuleSpaceBase = AlignUp((uptr)__end, PAGE_SIZE);
    s_Layout.ModuleSpaceSize = 0xFFFFFFFFFFFFFFFF - s_Layout.ModuleSpaceBase;

    s_Layout.KernelImageBase = (uptr)__start;
    s_Layout.KernelImageSize = __end - __start;
}

const Mm_VaLayout *Mm_GetVaLayout(void)
{
    return &s_Layout;
}

#define X86_PTE_PRESENT  (1ULL << 0)
#define X86_PTE_WRITABLE (1ULL << 1)
#define X86_PTE_PCD      (1ULL << 4)
#define X86_PTE_HUGE     (1ULL << 7)
#define X86_PTE_GLOBAL   (1ULL << 8)
#define X86_PTE_NX       (1ULL << 63)

#define X86_PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PML4_INDEX(va) (((va) >> 39) & 0x1FF)
#define PDPT_INDEX(va) (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)   (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)   (((va) >> 12) & 0x1FF)

static u64 *s_GetCurrentPml4(void)
{
    u64 cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    return Mm_PhysToVirt(cr3 & X86_PTE_ADDR_MASK);
}

static u64 s_TranslateFlags(u32 flags)
{
    u64 pte = X86_PTE_PRESENT;

    if (flags & MM_MAP_WRITE)
        pte |= X86_PTE_WRITABLE;
    if (flags & MM_MAP_NOEXEC)
        pte |= X86_PTE_NX;
    if (flags & MM_MAP_NOCACHE)
        pte |= X86_PTE_PCD;
    if (flags & MM_MAP_GLOBAL)
        pte |= X86_PTE_GLOBAL;

    return pte;
}

static u64 *s_EnsureTableLevel(u64 *table, usize index)
{
    if (table[index] & X86_PTE_PRESENT)
    {
        uptr phys = table[index] & X86_PTE_ADDR_MASK;
        return Mm_PhysToVirt(phys);
    }

    void *newTable = Mm_EarlyAllocate(PAGE_SIZE, PAGE_SIZE);
    if (!newTable)
        return nullptr;

    uptr newPhys = Mm_VirtToPhys(newTable);

    table[index] = newPhys | X86_PTE_PRESENT | X86_PTE_WRITABLE;

    return newTable;
}

bool Mm_EarlyMapPage(uptr virtualAddr, uptr physAddr, u32 flags)
{
    u64 *pml4 = s_GetCurrentPml4();

    u64 *pdpt = s_EnsureTableLevel(pml4, PML4_INDEX(virtualAddr));
    if (!pdpt)
        return false;

    u64 *pd = s_EnsureTableLevel(pdpt, PDPT_INDEX(virtualAddr));
    if (!pd)
        return false;

    u64 *pt = s_EnsureTableLevel(pd, PD_INDEX(virtualAddr));
    if (!pt)
        return false;

    usize index = PT_INDEX(virtualAddr);

    ASSERT(!(pt[index] & X86_PTE_PRESENT));

    pt[index] = (physAddr & X86_PTE_ADDR_MASK) | s_TranslateFlags(flags);

    return true;
}
