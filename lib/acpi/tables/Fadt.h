#ifndef LIB_ACPI_TABLES_FADT_H
#define LIB_ACPI_TABLES_FADT_H

#include <acpi/AcpiTypes.h>
#include <acpi/tables/GenericEntry.h>
#include <acpi/tables/Tables.h>

#define ACPI_FADT_HW_REDUCED    (1u << 20)
#define ACPI_FADT_LEGACY_DEVS   (1u << 0)
#define ACPI_FADT_VGA_NOT_PRES  (1u << 1)
#define ACPI_FADT_MSI_NOT_SUP   (1u << 3)
#define ACPI_FADT_CMOS_RTC_GONE (1u << 5)

typedef struct [[gnu::packed]]
{
    Acpi_SdtHeader Header;

    Acpi_Uint32 FirmwareCtrl;
    Acpi_Uint32 Dsdt;

    Acpi_Uint8  Reserved0;
    Acpi_Uint8  PreferredPmProfile;
    Acpi_Uint16 SciInterrupt;
    Acpi_Uint32 SmiCommand;
    Acpi_Uint8  AcpiEnable;
    Acpi_Uint8  AcpiDisable;
    Acpi_Uint8  S4BiosReq;
    Acpi_Uint8  PStateControl;

    Acpi_Uint32 Pm1aEventBlock;
    Acpi_Uint32 Pm1bEventBlock;
    Acpi_Uint32 Pm1aControlBlock;
    Acpi_Uint32 Pm1bControlBlock;
    Acpi_Uint32 Pm2ControlBlock;
    Acpi_Uint32 PmTimerBlock;
    Acpi_Uint32 Gpe0Block;
    Acpi_Uint32 Gpe1Block;

    Acpi_Uint8 Pm1EventLength;
    Acpi_Uint8 Pm1ControlLength;
    Acpi_Uint8 Pm2ControlLength;
    Acpi_Uint8 PmTimerLength;
    Acpi_Uint8 Gpe0BlockLength;
    Acpi_Uint8 Gpe1BlockLength;
    Acpi_Uint8 Gpe1Base;
    Acpi_Uint8 CstControl;

    Acpi_Uint16 Plvl2Latency;
    Acpi_Uint16 Plvl3Latency;
    Acpi_Uint16 FlushSize;
    Acpi_Uint16 FlushStride;

    Acpi_Uint8 DutyOffset;
    Acpi_Uint8 DutyWidth;
    Acpi_Uint8 DayAlarm;
    Acpi_Uint8 MonthAlarm;
    Acpi_Uint8 Century;

    Acpi_Uint16 IapcBootArch;
    Acpi_Uint8  Reserved1;
    Acpi_Uint32 Flags;

    Acpi_Gas    ResetRegister;
    Acpi_Uint8  ResetValue;
    Acpi_Uint16 ArmBootArch;
    Acpi_Uint8  FadtMinorVersion;

    Acpi_Uint64 XFirmwareCtrl;
    Acpi_Uint64 XDsdt;

    Acpi_Gas XPm1aEventBlock;
    Acpi_Gas XPm1bEventBlock;
    Acpi_Gas XPm1aControlBlock;
    Acpi_Gas XPm1bControlBlock;
    Acpi_Gas XPm2ControlBlock;
    Acpi_Gas XPmTimerBlock;
    Acpi_Gas XGpe0Block;
    Acpi_Gas XGpe1Block;

    Acpi_Gas SleepControlReg;
    Acpi_Gas SleepStatusReg;

    Acpi_Uint64 HypervisorVendor;
} Acpi_Fadt;

_Static_assert(sizeof(Acpi_Fadt) == 276, "Acpi_Fadt size mismatch");

typedef struct
{
    bool HwReduced;
    bool HasCmosRtc;
    bool Has8042;

    Acpi_Uint16 SciGsi;
    Acpi_Paddr  DsdtAddress;
    Acpi_Paddr  FacsAddress;
    Acpi_Uint8  PmProfile;
    Acpi_Uint16 BootArch;
    Acpi_Uint32 Flags;
} Acpi_FadtInfo;

Acpi_Status Acpi_FadtParse(const Acpi_Fadt *fadt, Acpi_FadtInfo *out);

const Acpi_Fadt *Acpi_FadtGet(void);

#endif /* LIB_ACPI_TABLES_FADT_H */
