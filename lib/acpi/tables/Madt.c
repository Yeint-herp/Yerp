#include <acpi/osi/OsServices.h>
#include <acpi/tables/Madt.h>

static const Acpi_Madt *s_MadtGet(void)
{
    return (const Acpi_Madt *)Acpi_FindTable(Acpi_SigMADT, 0);
}

Acpi_Usize Acpi_MadtEnumerate(Acpi_MadtType type, Acpi_MadtCallback cb, void *ctx)
{
    const Acpi_Madt *madt = s_MadtGet();
    if (!madt)
        return 0;

    Acpi_Usize        count  = 0;
    Acpi_Uint32       offset = sizeof(Acpi_Madt);
    const Acpi_Uint32 limit  = madt->Header.Length;

    while (offset + sizeof(Acpi_MadtEntry) <= limit)
    {
        const Acpi_MadtEntry *entry = (const Acpi_MadtEntry *)((const Acpi_Uint8 *)madt + offset);

        if (entry->Length < sizeof(Acpi_MadtEntry) || offset + entry->Length > limit)
            break;

        if (entry->Type == type)
        {
            count++;
            if (cb && !cb(entry, ctx))
                break;
        }

        offset += entry->Length;
    }

    return count;
}

Acpi_Paddr Acpi_MadtGetLocalApicAddress(void)
{
    const Acpi_Madt *madt = s_MadtGet();
    if (!madt)
        return 0;

    Acpi_Paddr addr = madt->LocalApicAddress;

    Acpi_Uint32 offset = sizeof(Acpi_Madt);
    Acpi_Uint32 limit  = madt->Header.Length;

    while (offset + sizeof(Acpi_MadtEntry) <= limit)
    {
        const Acpi_MadtEntry *entry = (const Acpi_MadtEntry *)((const Acpi_Uint8 *)madt + offset);

        if (entry->Length < sizeof(Acpi_MadtEntry) || offset + entry->Length > limit)
            break;

        if (entry->Type == kAcpiMadtLocalApicOverride)
        {
            const Acpi_MadtLocalApicOverride *ovr = (const Acpi_MadtLocalApicOverride *)entry;

            addr = ovr->Address;
        }

        offset += entry->Length;
    }

    return addr;
}
