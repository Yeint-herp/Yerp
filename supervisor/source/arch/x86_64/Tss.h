#ifndef SUPERVISOR_ARCH_X86_64_TSS_H
#define SUPERVISOR_ARCH_X86_64_TSS_H

struct [[gnu::packed]] X86_64_Tss
{
    u32 Reserved0;
    u64 Rsp0;
    u64 Rsp1;
    u64 Rsp2;
    u64 Reserved1;
    u64 Ist[7];
    u64 Reserved2;
    u16 Reserved3;
    u16 IopbOffset;
};

void X86_64_TssInit(struct X86_64_Tss *tss);

/// allocate ists and link them into the tss.
void X86_64_TssLateInit(struct X86_64_Tss *tss);

#endif /* SUPERVISOR_ARCH_X86_64_TSS_H */
