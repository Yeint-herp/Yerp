#ifndef LIB_ACPI_TABLES_GENERIC_ENTRY_H
#define LIB_ACPI_TABLES_GENERIC_ENTRY_H

#include <acpi/AcpiTypes.h>

typedef enum : Acpi_Uint8
{
    kAcpiAddrSpaceMemory  = 0x00,
    kAcpiAddrSpaceIo      = 0x01,
    kAcpiAddrSpacePci     = 0x02,
    kAcpiAddrSpaceEc      = 0x03,
    kAcpiAddrSpaceSmbus   = 0x04,
    kAcpiAddrSpaceCmos    = 0x05,
    kAcpiAddrSpacePciBar  = 0x06,
    kAcpiAddrSpaceIpmi    = 0x07,
    kAcpiAddrSpaceGpio    = 0x08,
    kAcpiAddrSpaceGserial = 0x09,
    kAcpiAddrSpacePcc     = 0x0A,
    kAcpiAddrSpacePrm     = 0x0B,
    kAcpiAddrSpaceFixedHw = 0x7F,
} Acpi_AddrSpace;

typedef enum : Acpi_Uint8
{
    kAcpiAccessUndefined = 0,
    kAcpiAccessByte      = 1,
    kAcpiAccessWord      = 2,
    kAcpiAccessDword     = 3,
    kAcpiAccessQword     = 4,
} Acpi_AccessSize;

typedef struct [[gnu::packed]]
{
    Acpi_AddrSpace AddressSpaceId;

    Acpi_Uint8 RegisterBitWidth;
    Acpi_Uint8 RegisterBitOffset;

    Acpi_AccessSize AccessSize;
    Acpi_Uint64     Address;
} Acpi_Gas;

_Static_assert(sizeof(Acpi_Gas) == 12, "Acpi_Gas size mismatch");

Acpi_Status Acpi_GasRead(const Acpi_Gas *gas, Acpi_Uint64 *value);
Acpi_Status Acpi_GasWrite(const Acpi_Gas *gas, Acpi_Uint64 value);

#endif /* LIB_ACPI_TABLES_GENERIC_ENTRY_H */
