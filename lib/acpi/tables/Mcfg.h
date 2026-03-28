#ifndef LIB_ACPI_TABLES_MCFG_H
#define LIB_ACPI_TABLES_MCFG_H

#include <acpi/Acpi.h>

typedef struct [[gnu::packed]]
{
    Acpi_Uint64 BaseAddress;
    Acpi_Uint16 PciSegmentGroup;

    Acpi_Uint8 StartBusNumber;
    Acpi_Uint8 EndBusNumber;

    Acpi_Uint32 Reserved;
} Acpi_McfgAllocation;

_Static_assert(sizeof(Acpi_McfgAllocation) == 16, "Acpi_McfgAllocation size mismatch");

typedef struct [[gnu::packed]]
{
    Acpi_SdtHeader Header;
    Acpi_Uint64    Reserved;
} Acpi_Mcfg;

Acpi_Usize Acpi_McfgGetCount(void);

const Acpi_McfgAllocation *Acpi_McfgGetAllocation(Acpi_Usize index);
const Acpi_McfgAllocation *Acpi_McfgFindSegment(Acpi_Uint16 segment, Acpi_Uint8 bus);

#endif /* LIB_ACPI_TABLES_MCFG_H */
