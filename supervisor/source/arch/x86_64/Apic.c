#define DBG_MODULE "Apic"

#include <acpi/tables/Madt.h>
#include <arch/Io.h>
#include <arch/MmArch.h>
#include <arch/x86_64/Apic.h>
#include <arch/x86_64/Cpuid.h>
#include <debug/DbgPrint.h>
#include <mm/Vas.h>

#define EX_TAG_APIC EX_TAG('A', 'p', 'i', 'c')

#define IOAPIC_REGSEL 0x00
#define IOAPIC_REGWIN 0x10

#define IOAPIC_REG_ID     0x00
#define IOAPIC_REG_VER    0x01
#define IOAPIC_REG_ARB    0x02
#define IOAPIC_REG_REDTBL 0x10

static u32 s_IoApicReadReg(uptr base, u8 reg)
{
    Arch_MmioWrite32((void *)(base + IOAPIC_REGSEL), reg);
    return Arch_MmioRead32((void *)(base + IOAPIC_REGWIN));
}

static X86_64_ApicState s_ApicState;

static bool s_OnLocalApic(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtLocalApic *la = (const Acpi_MadtLocalApic *)entry;

    if (!(la->Flags & (ACPI_MADT_LAPIC_ENABLED | ACPI_MADT_LAPIC_ONLINE_CAP)))
        return true;

    X86_64_ApicLapicInfo info = {
        .AcpiProcessorId = la->AcpiProcessorId,
        .ApicId          = la->ApicId,
        .Flags           = la->Flags,
    };

    Dsa_VectorPush(s_ApicState.Lapics, info, EX_TAG_APIC);
    Log(TRACE, "LAPIC: acpiId %u, apicId %u, flags %#x", info.AcpiProcessorId, info.ApicId, info.Flags);

    return true;
}

static bool s_OnX2Apic(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtLocalX2Apic *x2 = (const Acpi_MadtLocalX2Apic *)entry;

    if (!(x2->Flags & (ACPI_MADT_LAPIC_ENABLED | ACPI_MADT_LAPIC_ONLINE_CAP)))
        return true;

    X86_64_ApicLapicInfo info = {
        .AcpiProcessorId = (u8)x2->AcpiProcessorUid,
        .ApicId          = x2->X2ApicId,
        .Flags           = x2->Flags,
    };

    Dsa_VectorPush(s_ApicState.Lapics, info, EX_TAG_APIC);

    Log(TRACE, "x2APIC: uid %u, x2apicId %u, flags %#x", x2->AcpiProcessorUid, x2->X2ApicId, x2->Flags);

    return true;
}

static bool s_OnIoApic(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtIoApic *io = (const Acpi_MadtIoApic *)entry;

    X86_64_ApicIoApicInfo info = {
        .IoApicId = io->IoApicId,
        .GsiBase  = io->GsiBase,
        .PhysBase = io->IoApicAddress,
        .GsiCount = 0,
        .MmioBase = 0,
    };

    info.MmioBase = Mm_MapIoSpace(info.PhysBase, 0x1000, kMmCacheUncached);

    if (info.MmioBase)
    {
        u32 ver       = s_IoApicReadReg(info.MmioBase, IOAPIC_REG_VER);
        info.GsiCount = ((ver >> 16) & 0xFF) + 1;
    }
    else
    {
        Log(ERROR, "failed to map IOAPIC %u at %#llx", info.IoApicId, info.PhysBase);
        return true;
    }

    Dsa_VectorPush(s_ApicState.IoApics, info, EX_TAG_APIC);
    Log(TRACE, "IOAPIC: id %u, GsiBase %u, GsiCount %u, phys %#llx", info.IoApicId, info.GsiBase, info.GsiCount,
        info.PhysBase);

    return true;
}

static bool s_OnIso(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtIso *iso = (const Acpi_MadtIso *)entry;

    X86_64_ApicIsoInfo info = {
        .Bus    = iso->Bus,
        .Source = iso->Source,
        .Gsi    = iso->Gsi,
        .Flags  = iso->Flags,
    };

    Dsa_VectorPush(s_ApicState.Isos, info, EX_TAG_APIC);
    Log(TRACE, "ISO: bus %u, source (IRQ %u) -> GSI %u, flags %#x", info.Bus, info.Source, info.Gsi, info.Flags);

    return true;
}

static bool s_OnLocalApicNmi(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtLocalApicNmi *nmi = (const Acpi_MadtLocalApicNmi *)entry;

    X86_64_ApicNmiInfo info = {
        .Lint  = nmi->Lint,
        .Flags = nmi->Flags,
    };

    Dsa_VectorPush(s_ApicState.Nmis, info, EX_TAG_APIC);
    Log(TRACE, "LAPIC NMI: LINT%u, flags %#x", info.Lint, info.Flags);

    return true;
}

static bool s_OnLapicOverride(const Acpi_MadtEntry *entry, void *ctx)
{
    (void)ctx;
    const Acpi_MadtLocalApicOverride *ovr = (const Acpi_MadtLocalApicOverride *)entry;

    Log(INFO, "LAPIC address override: %#llx -> %#llx", s_ApicState.LapicBase, ovr->Address);

    s_ApicState.LapicBase = ovr->Address;
    return true;
}

void X86_64_ApicDiscover(void)
{
    Core_ZeroMemory(&s_ApicState, sizeof(s_ApicState));

    s_ApicState.LapicBase = Acpi_MadtGetLocalApicAddress();

    X86_64_CpuidRegs regs;
    X86_64_CpuidQuery(1, 0, &regs);
    s_ApicState.x2apic = (regs.Ecx >> 21) & 1;

    Log(INFO, "LAPIC base %#llx, x2APIC hw %s", s_ApicState.LapicBase,
        s_ApicState.x2apic ? "available" : "not available");

    Acpi_MadtEnumerate(kAcpiMadtLocalApic, s_OnLocalApic, nullptr);
    Acpi_MadtEnumerate(kAcpiMadtLocalX2Apic, s_OnX2Apic, nullptr);
    Acpi_MadtEnumerate(kAcpiMadtIoApic, s_OnIoApic, nullptr);
    Acpi_MadtEnumerate(kAcpiMadtIso, s_OnIso, nullptr);
    Acpi_MadtEnumerate(kAcpiMadtLocalApicNmi, s_OnLocalApicNmi, nullptr);
    Acpi_MadtEnumerate(kAcpiMadtLocalApicOverride, s_OnLapicOverride, nullptr);

    Log(INFO, "discovered %u LAPICs, %u IOAPICs, %u ISOs, %u NMIs", Dsa_VectorCount(s_ApicState.Lapics),
        Dsa_VectorCount(s_ApicState.IoApics), Dsa_VectorCount(s_ApicState.Isos), Dsa_VectorCount(s_ApicState.Nmis));
}

const X86_64_ApicState *X86_64_ApicGetState(void)
{
    return &s_ApicState;
}

u32 X86_64_ApicIsaIrqToGsi(u8 isaIrq, u16 *outFlags)
{
    Dsa_VectorForEach(s_ApicState.Isos, it)
    {
        if (it->Source == isaIrq)
        {
            if (outFlags)
                *outFlags = it->Flags;

            return it->Gsi;
        }
    }

    if (outFlags)
        *outFlags = 0;

    return isaIrq;
}

const X86_64_ApicIoApicInfo *X86_64_ApicFindIoApicForGsi(u32 gsi)
{
    Dsa_VectorForEach(s_ApicState.IoApics, it)
    {
        if (gsi >= it->GsiBase && gsi < it->GsiBase + it->GsiCount)
            return it;
    }

    return nullptr;
}
