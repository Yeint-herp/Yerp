#include <acpi/tables/Mcfg.h>

static const Acpi_Mcfg *s_McfgGet(void)
{
    return (const Acpi_Mcfg *)Acpi_FindTable(Acpi_SigMCFG, 0);
}

Acpi_Usize Acpi_McfgGetCount(void)
{
    const Acpi_Mcfg *mcfg = s_McfgGet();
    if (!mcfg)
        return 0;

    Acpi_Uint32 dataLen = mcfg->Header.Length - sizeof *mcfg;
    return dataLen / sizeof(Acpi_McfgAllocation);
}

const Acpi_McfgAllocation *Acpi_McfgGetAllocation(Acpi_Usize index)
{
    const Acpi_Mcfg *mcfg = s_McfgGet();
    if (!mcfg || index >= Acpi_McfgGetCount())
        return nullptr;

    const Acpi_McfgAllocation *entries = (const Acpi_McfgAllocation *)((const Acpi_Uint8 *)mcfg + sizeof *mcfg);

    return &entries[index];
}

const Acpi_McfgAllocation *Acpi_McfgFindSegment(Acpi_Uint16 segment, Acpi_Uint8 bus)
{
    Acpi_Usize count = Acpi_McfgGetCount();

    for (Acpi_Usize i = 0; i < count; i++)
    {
        const Acpi_McfgAllocation *alloc = Acpi_McfgGetAllocation(i);
        if (alloc->PciSegmentGroup == segment && bus >= alloc->StartBusNumber && bus <= alloc->EndBusNumber)
            return alloc;
    }

    return nullptr;
}
