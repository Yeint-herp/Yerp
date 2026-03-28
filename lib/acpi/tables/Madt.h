#ifndef LIB_ACPI_TABLES_MADT_H
#define LIB_ACPI_TABLES_MADT_H

#include <acpi/Acpi.h>

typedef enum : Acpi_Uint8
{
    kAcpiMadtLocalApic         = 0x00,
    kAcpiMadtIoApic            = 0x01,
    kAcpiMadtIso               = 0x02,
    kAcpiMadtNmiSource         = 0x03,
    kAcpiMadtLocalApicNmi      = 0x04,
    kAcpiMadtLocalApicOverride = 0x05,
    kAcpiMadtLocalX2Apic       = 0x09,
    kAcpiMadtLocalX2ApicNmi    = 0x0A,
    kAcpiMadtGicc              = 0x0B,
    kAcpiMadtGicd              = 0x0C,
    kAcpiMadtGicMsiFrame       = 0x0D,
    kAcpiMadtGicRedist         = 0x0E,
    kAcpiMadtGicIts            = 0x0F,
    kAcpiMadtRiscvIntc         = 0x18,
} Acpi_MadtType;

typedef struct [[gnu::packed]]
{
    Acpi_SdtHeader Header;
    Acpi_Uint32    LocalApicAddress;
    Acpi_Uint32    Flags;
} Acpi_Madt;

typedef struct [[gnu::packed]]
{
    Acpi_MadtType Type;
    Acpi_Uint8    Length;
} Acpi_MadtEntry;

#define ACPI_MADT_LAPIC_ENABLED    (1u << 0)
#define ACPI_MADT_LAPIC_ONLINE_CAP (1u << 1)

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint8     AcpiProcessorId;
    Acpi_Uint8     ApicId;
    Acpi_Uint32    Flags;
} Acpi_MadtLocalApic;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint8     IoApicId;
    Acpi_Uint8     Reserved;
    Acpi_Uint32    IoApicAddress;
    Acpi_Uint32    GsiBase;
} Acpi_MadtIoApic;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint8     Bus;
    Acpi_Uint8     Source;
    Acpi_Uint32    Gsi;
    Acpi_Uint16    Flags;
} Acpi_MadtIso;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint8     AcpiProcessorId;
    Acpi_Uint16    Flags;
    Acpi_Uint8     Lint;
} Acpi_MadtLocalApicNmi;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint16    Reserved;
    Acpi_Uint64    Address;
} Acpi_MadtLocalApicOverride;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint16    Reserved;
    Acpi_Uint32    X2ApicId;
    Acpi_Uint32    Flags;
    Acpi_Uint32    AcpiProcessorUid;
} Acpi_MadtLocalX2Apic;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint16    Reserved;
    Acpi_Uint32    CpuInterfaceNumber;
    Acpi_Uint32    AcpiProcessorUid;
    Acpi_Uint32    Flags;
    Acpi_Uint32    ParkingProtocolVersion;
    Acpi_Uint32    PerformanceInterruptGsiv;
    Acpi_Uint64    ParkedAddress;
    Acpi_Uint64    PhysicalBaseAddress;
    Acpi_Uint64    Gicv;
    Acpi_Uint64    Gich;
    Acpi_Uint32    VgicMaintenanceInterrupt;
    Acpi_Uint64    GicrBaseAddress;
    Acpi_Uint64    Mpidr;
    Acpi_Uint8     ProcessorPowerEfficiencyClass;
    Acpi_Uint8     Reserved2;
    Acpi_Uint16    SpeOverflowInterrupt;
} Acpi_MadtGicc;

typedef struct [[gnu::packed]]
{
    Acpi_MadtEntry Header;
    Acpi_Uint16    Reserved;
    Acpi_Uint32    GicId;
    Acpi_Uint64    PhysicalBaseAddress;
    Acpi_Uint32    SystemVectorBase;
    Acpi_Uint8     GicVersion;
    Acpi_Uint8     Reserved2[3];
} Acpi_MadtGicd;

typedef bool (*Acpi_MadtCallback)(const Acpi_MadtEntry *entry, void *ctx);

Acpi_Usize Acpi_MadtEnumerate(Acpi_MadtType type, Acpi_MadtCallback cb, void *ctx);
Acpi_Paddr Acpi_MadtGetLocalApicAddress(void);

#endif /* LIB_ACPI_TABLES_MADT_H */
