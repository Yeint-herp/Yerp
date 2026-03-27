#ifndef SUPERVISOR_ARCH_X86_64_GDT_H
#define SUPERVISOR_ARCH_X86_64_GDT_H

struct X86_64_Tss;

struct [[gnu::packed]] X86_64_GdtEntry
{
    u16 LimitLow;
    u16 BaseLow;
    u8  BaseMid;
    u8  Access;
    u8  Granularity;
    u8  BaseHigh;
};

struct [[gnu::packed]] X86_64_TssDescriptor
{
    u16 LimitLow;
    u16 BaseLow;
    u8  BaseMid;
    u8  Access;
    u8  Granularity;
    u8  BaseHigh;
    u32 BaseUpper;
    u32 Reserved;
};

struct [[gnu::packed]] X86_64_Gdt
{
    struct X86_64_GdtEntry      Null;
    struct X86_64_GdtEntry      SupervisorCode;
    struct X86_64_GdtEntry      SupervisorData;
    struct X86_64_GdtEntry      UserCode32;
    struct X86_64_GdtEntry      UserData;
    struct X86_64_GdtEntry      UserCode64;
    struct X86_64_TssDescriptor Tss;
};

void X86_64_GdtInit(struct X86_64_Gdt *gdt, struct X86_64_Tss *tss);

#endif /* SUPERVISOR_ARCH_X86_64_GDT_H */
