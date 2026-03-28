#include <acpi/tables/Hpet.h>

const Acpi_Hpet *Acpi_HpetGet(void)
{
    return (const Acpi_Hpet *)Acpi_FindTable(Acpi_SigHPET, 0);
}

Acpi_Paddr Acpi_HpetGetBaseAddress(void)
{
    const Acpi_Hpet *hpet = Acpi_HpetGet();
    if (!hpet)
        return 0;

    return hpet->BaseAddress.Address;
}
