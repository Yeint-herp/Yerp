#ifndef LIB_ACPI_TABLES_TABLES_H
#define LIB_ACPI_TABLES_TABLES_H

#include <acpi/AcpiTypes.h>

typedef struct [[gnu::packed]]
{
    char       Signature[8];
    Acpi_Uint8 Checksum;
    char       OemId[6];
    Acpi_Uint8 Revision;

    Acpi_Uint32 RsdtAddress;

    Acpi_Uint32 Length;

    Acpi_Uint64 XsdtAddress;

    Acpi_Uint8 ExtendedChecksum;
    Acpi_Uint8 Reserved[3];
} Acpi_Rsdp;

_Static_assert(sizeof(Acpi_Rsdp) == 36, "Acpi_Rsdp size mismatch");

typedef struct [[gnu::packed]]
{
    Acpi_Uint32 Signature;
    Acpi_Uint32 Length;
    Acpi_Uint8  Revision;
    Acpi_Uint8  Checksum;

    char        OemId[6];
    char        OemTableId[8];
    Acpi_Uint32 OemRevision;

    Acpi_Uint32 CreatorId;
    Acpi_Uint32 CreatorRevision;
} Acpi_SdtHeader;

_Static_assert(sizeof(Acpi_SdtHeader) == 36, "Acpi_SdtHeader size mismatch");

#define ACPI_MAX_TABLES 64

typedef struct
{
    Acpi_Uint32           Signature;
    Acpi_Paddr            PhysAddr;
    const Acpi_SdtHeader *Mapped;
} Acpi_TableEntry;

typedef struct
{
    Acpi_Rsdp       Rsdp;
    bool            UseXsdt;
    Acpi_Usize      Count;
    Acpi_TableEntry Entries[ACPI_MAX_TABLES];
} Acpi_TableContext;

Acpi_Status Acpi_ValidateChecksum(const void *data, Acpi_Usize length);
Acpi_Status Acpi_ValidateRsdp(const Acpi_Rsdp *rsdp);

Acpi_Status Acpi_TablesInit(Acpi_TableContext *ctx);

const Acpi_SdtHeader *Acpi_TablesFind(Acpi_TableContext *ctx, Acpi_Uint32 signature, Acpi_Usize index);

#endif /* LIB_ACPI_TABLES_TABLES_H */
