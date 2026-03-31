#define DBG_MODULE "Interrupt"

#include <arch/CoreLocal.h>
#include <arch/CpuHint.h>
#include <arch/Interrupts.h>
#include <arch/Io.h>
#include <arch/x86_64/Apic.h>
#include <arch/x86_64/Hpet.h>
#include <arch/x86_64/IoApic.h>
#include <arch/x86_64/LocalApic.h>
#include <arch/x86_64/Msr.h>
#include <arch/x86_64/Spcr.h>
#include <core/Spcb.h>
#include <core/Spinlock.h>
#include <debug/DbgPrint.h>
#include <debug/Panic.h>
#include <executive/Init.h>

static Arch_Atomic32 s_VectorBitmap[8] = {0xFFFFFFFF};
static Core_Spinlock s_AllocLock;

Interrupt_Handle Interrupt_Allocate(Irql_t irql)
{
    if (irql < IRQL_DIRQL_MIN || irql > IRQL_IPI)
    {
        Log(ERROR, "Interrupt_Allocate: IRQL %u out of allocatable range", irql);
        return INTERRUPT_HANDLE_NULL;
    }

    const u32 base = irql * 16;
    const u32 top  = base + 15;

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 vec = base; vec <= top; vec++)
    {
        if (!Bitmap_TestBit(s_VectorBitmap, vec))
        {
            Bitmap_SetBit(s_VectorBitmap, vec);
            Core_SpinlockRelease(&s_AllocLock);

            Log(TRACE, "allocated vector %u for IRQL %u", vec, irql);
            return vec;
        }
    }

    Core_SpinlockRelease(&s_AllocLock);

    Log(ERROR, "Interrupt_Allocate: no free vectors for IRQL %u (range %u-%u)", irql, base, top);
    return INTERRUPT_HANDLE_NULL;
}

void Interrupt_Free(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256 || vec < 32)
    {
        Log(ERROR, "Interrupt_Free: invalid handle %u", vec);
        return;
    }

    struct Core_SPCB *spcb = Arch_GetCurrentSpcb();
    Arch_VectorInfo  *vi   = &spcb->ArchData.VectorInfo[vec];

    if (vi->IsHwIrq)
    {
        X86_64_IoApicMaskGsi(vi->Gsi);
        vi->IsHwIrq = false;
        vi->Gsi     = 0;
    }

    Core_SpinlockAcquire(&s_AllocLock);
    Bitmap_ClearBit(s_VectorBitmap, vec);
    Core_SpinlockRelease(&s_AllocLock);

    Log(TRACE, "freed vector %u", vec);
}

Irql_t Interrupt_GetIrql(Interrupt_Handle handle)
{
    return (Irql_t)((u32)handle >> 4);
}

bool Interrupt_Register(Interrupt_Handle handle, Interrupt_Handler handler, void *context)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    struct Core_SPCB           *spcb = Arch_GetCurrentSpcb();
    Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

    if (reg->Routine != (uptr)Exec_DefaultInterruptHandler)
    {
        Log(ERROR, "vector %u already registered on core %u", vec, spcb->ProcessorNumber);
        return false;
    }

    reg->Context = context;
    Arch_CompilerBarrier();
    reg->Routine = (uptr)handler;

    return true;
}

bool Interrupt_Deregister(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    struct Core_SPCB           *spcb = Arch_GetCurrentSpcb();
    Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

    reg->Routine = (uptr)Exec_DefaultInterruptHandler;
    Arch_CompilerBarrier();
    reg->Context = nullptr;

    return true;
}

bool Interrupt_RegisterGlobal(Interrupt_Handle handle, Interrupt_Handler handler, void *context)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    const u32 cpuCount = Core_GetProcessorCount();

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB *spcb = Core_SpcbGetByNumber(i);
        if (spcb->ArchData.IsrTable[vec].Routine != (uptr)Exec_DefaultInterruptHandler)
        {
            Log(ERROR, "vector %u already registered on core %u, aborting global registration", vec,
                spcb->ProcessorNumber);

            Core_SpinlockRelease(&s_AllocLock);
            return false;
        }
    }

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB           *spcb = Core_SpcbGetByNumber(i);
        Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

        reg->Context = context;
        Arch_CompilerBarrier();
        reg->Routine = (uptr)handler;
    }

    Core_SpinlockRelease(&s_AllocLock);

    Log(TRACE, "globally registered vector %u on %u cores", vec, cpuCount);
    return true;
}

bool Interrupt_DeregisterGlobal(Interrupt_Handle handle)
{
    const u32 vec = handle;

    if (vec >= 256)
        return false;

    const u32 cpuCount = Core_GetProcessorCount();

    Core_SpinlockAcquire(&s_AllocLock);

    for (u32 i = 0; i < cpuCount; i++)
    {
        struct Core_SPCB           *spcb = Core_SpcbGetByNumber(i);
        Arch_InterruptRegistration *reg  = &spcb->ArchData.IsrTable[vec];

        reg->Routine = (uptr)Exec_DefaultInterruptHandler;
        Arch_CompilerBarrier();
        reg->Context = nullptr;
    }

    Core_SpinlockRelease(&s_AllocLock);
    return true;
}

Interrupt_Handle Interrupt_AllocateHwIrq(Interrupt_HwIrq hwIrq, Interrupt_Flags flags, Irql_t irql)
{
    const u32 gsi = hwIrq;

    const X86_64_ApicIoApicInfo *io = X86_64_ApicFindIoApicForGsi(gsi);
    if (!io)
    {
        Log(ERROR, "AllocateHwIrq: no IOAPIC for GSI %u", gsi);
        return INTERRUPT_HANDLE_NULL;
    }

    Interrupt_Handle handle = Interrupt_Allocate(irql);
    if (handle == INTERRUPT_HANDLE_NULL)
        return INTERRUPT_HANDLE_NULL;

    struct Core_SPCB       *spcb  = Arch_GetCurrentSpcb();
    const X86_64_ApicState *state = X86_64_ApicGetState();

    u32 destApicId = 0;
    Dsa_VectorForEach(state->Lapics, it)
    {
        if (it->AcpiProcessorId == spcb->ProcessorNumber || it->ApicId == spcb->ArchId)
        {
            destApicId = it->ApicId;
            break;
        }
    }

    if (!X86_64_IoApicRouteGsi(gsi, handle, flags, destApicId, spcb->ProcessorNumber))
    {
        Interrupt_Free(handle);
        return INTERRUPT_HANDLE_NULL;
    }

    Arch_VectorInfo *vi = &spcb->ArchData.VectorInfo[handle];
    vi->Gsi             = gsi;
    vi->IsHwIrq         = true;

    Log(TRACE, "HwIrq: GSI %u -> vector %u, IRQL %u, core %u (APIC %u)", gsi, handle, irql, spcb->ProcessorNumber,
        destApicId);

    return handle;
}

void Interrupt_Mask(Interrupt_Handle handle)
{
    if (handle >= 256)
        return;

    struct Core_SPCB *spcb = Arch_GetCurrentSpcb();
    Arch_VectorInfo  *vi   = &spcb->ArchData.VectorInfo[handle];

    if (vi->IsHwIrq)
        X86_64_IoApicMaskGsi(vi->Gsi);
}

void Interrupt_Unmask(Interrupt_Handle handle)
{
    if (handle >= 256)
        return;

    struct Core_SPCB *spcb = Arch_GetCurrentSpcb();
    Arch_VectorInfo  *vi   = &spcb->ArchData.VectorInfo[handle];

    if (vi->IsHwIrq)
        X86_64_IoApicUnmaskGsi(vi->Gsi);
}

void Interrupt_SendEoi(Interrupt_Handle)
{
    X86_64_LocalApicSendEoi();
}

void Interrupt_SendIpi(Ipi_Target target, u32 processorNumber, Interrupt_Handle handle)
{
    const u8 vector = handle;

    u32 icrLo      = vector | LAPIC_LVT_DELIV_FIXED;
    u32 destApicId = 0;

    switch (target)
    {
        case kIpiTargetSelf:
            icrLo |= 1u << 18;
            break;

        case kIpiTargetAll:
            icrLo |= 2u << 18;
            break;

        case kIpiTargetAllExcludingSelf:
            icrLo |= 3u << 18;
            break;

        case kIpiTargetSpecific:
        {
            const X86_64_ApicState *state = X86_64_ApicGetState();
            bool                    found = false;

            Dsa_VectorForEach(state->Lapics, it)
            {
                if (it->AcpiProcessorId == processorNumber || it->ApicId == processorNumber)
                {
                    destApicId = it->ApicId;
                    found      = true;
                    break;
                }
            }

            if (!found)
            {
                Log(ERROR, "SendIpi: no APIC ID for processor %u", processorNumber);
                return;
            }
            break;
        }

        default:
            Log(ERROR, "SendIpi: invalid target %u", target);
            return;
    }

    const X86_64_ApicState *state = X86_64_ApicGetState();

    if (state->x2apic)
    {
        u64 icr = ((u64)destApicId << 32) | icrLo;
        X86_64_WriteMsr(X2APIC_MSR_ICR, icr);
    }
    else
    {
        while (X86_64_LocalApicReadReg(LAPIC_REG_ICR_LO) & (1u << 12))
            Arch_CpuRelax();

        X86_64_LocalApicWriteReg(LAPIC_REG_ICR_HI, destApicId << 24);
        X86_64_LocalApicWriteReg(LAPIC_REG_ICR_LO, icrLo);
    }

    Log(TRACE, "IPI: target = %u proc = %u vector = %u dest_apic = %u", target, processorNumber, vector, destApicId);
}

static void s_DisablePic8259(void)
{
    Arch_IoOut8(0x20, 0x11);
    Arch_IoOut8(0xA0, 0x11);
    Arch_IoOut8(0x21, 0x20);
    Arch_IoOut8(0xA1, 0x28);
    Arch_IoOut8(0x21, 0x04);
    Arch_IoOut8(0xA1, 0x02);
    Arch_IoOut8(0x21, 0x01);
    Arch_IoOut8(0xA1, 0x01);

    /// mask all lines.
    Arch_IoOut8(0x21, 0xFF);
    Arch_IoOut8(0xA1, 0xFF);

    Log(TRACE, "8259 PIC disabled");
}

void Interrupt_ControllerInit(void)
{
    X86_64_ApicDiscover();
    s_DisablePic8259();

    if (!X86_64_HpetInit())
        Panic("HPET init failed, cannot calibrate LAPIC timer");

    X86_64_LocalApicInit();
    X86_64_IoApicInit();
    X86_64_LocalApicCalibrateTimer();

    Log(INFO, "interrupt controller initialized");
}
