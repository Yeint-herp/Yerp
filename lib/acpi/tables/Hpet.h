#ifndef LIB_ACPI_TABLES_HPET_H
#define LIB_ACPI_TABLES_HPET_H

#include <acpi/Acpi.h>
#include <acpi/tables/GenericEntry.h>

typedef struct [[gnu::packed]]
{
    Acpi_SdtHeader Header;
    Acpi_Uint32    EventTimerBlockId;
    Acpi_Gas       BaseAddress;
    Acpi_Uint8     HpetNumber;
    Acpi_Uint16    MinClockTick;
    Acpi_Uint8     PageProtection;
} Acpi_Hpet;

_Static_assert(sizeof(Acpi_Hpet) == 56, "Acpi_Hpet size mismatch");

const Acpi_Hpet *Acpi_HpetGet(void);
Acpi_Paddr       Acpi_HpetGetBaseAddress(void);

#endif /* LIB_ACPI_TABLES_HPET_H */
