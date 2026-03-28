#include <arch/Spcr.h>
#include <core/Spcb.h>

#define DEFINE_EQU(name, type, member) __asm__("\n#MAGIC_OFFSET# .equ " #name ", %c0" : : "i"(offsetof(type, member)))
#define DEFINE_SIZE(name, type)        __asm__("\n#MAGIC_OFFSET# .equ " #name ", %c0" : : "i"(sizeof(type)))

void generate_offsets(void)
{
    DEFINE_EQU(SPCB_SELF_OFFSET, struct Core_SPCB, Self);
    DEFINE_EQU(SPCB_PROCESSOR_NUM_OFFSET, struct Core_SPCB, ProcessorNumber);
    DEFINE_EQU(SPCB_CURRENT_IRQL_OFFSET, struct Core_SPCB, CurrentIrql);
    DEFINE_EQU(SPCB_SCRATCH_ZONE_OFFSET, struct Core_SPCB, ScratchZone);
    DEFINE_EQU(SPCB_ARCH_DATA_OFFSET, struct Core_SPCB, ArchData);

    DEFINE_SIZE(SPCB_STRUCT_SIZE, struct Core_SPCB);

#if kArch == x86_64
    DEFINE_EQU(X86_64_SPCB_TSS_OFFSET, struct Core_SPCB, ArchData.Tss);
    DEFINE_EQU(X86_64_SPCB_GDT_OFFSET, struct Core_SPCB, ArchData.Gdt);
    DEFINE_EQU(X86_64_SPCB_IDT_OFFSET, struct Core_SPCB, ArchData.Idt);
    DEFINE_EQU(X86_64_SPCB_ISR_TABLE_OFFSET, struct Core_SPCB, ArchData.IsrTable);

    DEFINE_EQU(X86_64_SPCR_TSS_OFFSET, struct Arch_SPCR, Tss);
    DEFINE_EQU(X86_64_SPCR_ISR_TABLE_OFFSET, struct Arch_SPCR, IsrTable);
#else
#error "Unsupported architecture for offset generation"
#endif
}
