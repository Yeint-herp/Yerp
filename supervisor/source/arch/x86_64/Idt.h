#ifndef SUPERVISOR_ARCH_X86_64_IDT_H
#define SUPERVISOR_ARCH_X86_64_IDT_H

struct [[gnu::packed]] X86_64_IdtEntry
{
    u16 OffsetLow;
    u16 Selector;
    u8  Ist;
    u8  TypeAttr;
    u16 OffsetMid;
    u32 OffsetHigh;
    u32 Reserved;
};

struct [[gnu::packed]] X86_64_Idt
{
    struct X86_64_IdtEntry Entries[256];
};

void X86_64_IdtInit(struct X86_64_Idt *idt);

#endif /* SUPERVISOR_ARCH_X86_64_IDT_H */
