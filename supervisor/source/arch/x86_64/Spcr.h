#ifndef SUPERVISOR_HARCH_X86_64_SPCR_H
#define SUPERVISOR_HARCH_X86_64_SPCR_H

#include <arch/Interrupts.h>
#include <arch/x86_64/Gdt.h>
#include <arch/x86_64/Idt.h>
#include <arch/x86_64/Tss.h>

typedef struct
{
    uptr  Routine;
    void *Context;
} Arch_InterruptRegistration;

typedef struct
{
    u32  Gsi;
    bool IsHwIrq;
} Arch_VectorInfo;

struct Arch_SPCR
{
    struct X86_64_Tss Tss;
    struct X86_64_Gdt Gdt;

    struct X86_64_Idt          Idt;
    Arch_InterruptRegistration IsrTable[256];
    Arch_VectorInfo            VectorInfo[256];
};

struct limine_mp_response;
struct limine_mp_info;

bool Arch_SpcrInit(struct Arch_SPCR *spcr);
bool Arch_SpcrLateInit(struct Arch_SPCR *spcr);

typedef u32 ArchId_t;

ArchId_t Arch_GetBspArchId(const struct limine_mp_response *mpResponse);
ArchId_t Arch_GetArchId(const struct limine_mp_info *mpInfo);

#endif /* SUPERVISOR_HARCH_X86_64_SPCR_H */
