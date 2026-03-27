#include <arch/x86_64/Gdt.h>
#include <arch/x86_64/Idt.h>
#include <arch/x86_64/Spcr.h>
#include <arch/x86_64/Tss.h>
#include <core/Memory.h>
#include <core/Spcb.h>

extern const uptr X86_64_IsrStubTable[256];

struct [[gnu::packed]] s_DescriptorPointer
{
    u16 Limit;
    u64 Base;
};

static void s_SetGdtEntry(struct X86_64_GdtEntry *entry, u8 access, u8 flags)
{
    entry->LimitLow    = 0xFFFF;
    entry->BaseLow     = 0;
    entry->BaseMid     = 0;
    entry->Access      = access;
    entry->Granularity = flags | 0x0F;
    entry->BaseHigh    = 0;
}

static void s_SetIdtEntry(struct X86_64_IdtEntry *entry, uptr isr, u8 typeAttr, u8 ist)
{
    entry->OffsetLow  = isr & 0xFFFF;
    entry->Selector   = 0x08;
    entry->Ist        = ist;
    entry->TypeAttr   = typeAttr;
    entry->OffsetMid  = (isr >> 16) & 0xFFFF;
    entry->OffsetHigh = (isr >> 32) & 0xFFFFFFFF;
    entry->Reserved   = 0;
}

void X86_64_TssInit(struct X86_64_Tss *tss)
{
    Core_ZeroMemory(tss, sizeof *tss);

    tss->IopbOffset = sizeof *tss;
}

void X86_64_GdtInit(struct X86_64_Gdt *gdt, struct X86_64_Tss *tss)
{
    Core_ZeroMemory(&gdt->Null, sizeof gdt->Null);

    s_SetGdtEntry(&gdt->SupervisorCode, 0x9A, 0x20);

    s_SetGdtEntry(&gdt->SupervisorData, 0x92, 0x00);

    s_SetGdtEntry(&gdt->UserCode32, 0xFA, 0x40);

    s_SetGdtEntry(&gdt->UserData, 0xF2, 0x00);

    s_SetGdtEntry(&gdt->UserCode64, 0xFA, 0x20);

    const u64 tssBase  = (uptr)tss;
    const u64 gdtBase  = (uptr)gdt;
    const u32 tssLimit = sizeof *tss - 1;
    const u32 gdtLimit = sizeof *gdt - 1;

    gdt->Tss.LimitLow    = tssLimit & 0xFFFF;
    gdt->Tss.BaseLow     = tssBase & 0xFFFF;
    gdt->Tss.BaseMid     = (tssBase >> 16) & 0xFF;
    gdt->Tss.Access      = 0x89;
    gdt->Tss.Granularity = (tssLimit >> 16) & 0x0F;
    gdt->Tss.BaseHigh    = (tssBase >> 24) & 0xFF;
    gdt->Tss.BaseUpper   = tssBase >> 32;
    gdt->Tss.Reserved    = 0;

    struct s_DescriptorPointer gdtr = {.Limit = gdtLimit, .Base = gdtBase};

    __asm__ volatile("lgdt %0\n"
                     "push $0x08\n"
                     "lea 1f(%%rip), %%rax\n"
                     "push %%rax\n"
                     "lretq\n"
                     "1:\n"
                     "mov $0x10, %%ax\n"
                     "mov %%ax, %%ds\n"
                     "mov %%ax, %%es\n"
                     "mov %%ax, %%ss\n"
                     "mov $0x00, %%ax\n"
                     "mov %%ax, %%fs\n"
                     "mov %%ax, %%gs\n"
                     :
                     : "m"(gdtr)
                     : "rax", "memory");

    __asm__ volatile("ltr %%ax" : : "a"(0x30) : "memory");
}

void X86_64_IdtInit(struct X86_64_Idt *idt)
{
    for (int i = 0; i < 256; i++)
        s_SetIdtEntry(&idt->Entries[i], X86_64_IsrStubTable[i], 0x8E, 0);

    const u64 idtBase  = (uptr)idt;
    const u32 idtLimit = sizeof *idt - 1;

    struct s_DescriptorPointer idtr = {.Limit = idtLimit, .Base = idtBase};

    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}
