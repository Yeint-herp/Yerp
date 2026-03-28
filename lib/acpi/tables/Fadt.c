#include <acpi/Acpi.h>
#include <acpi/osi/OsServices.h>
#include <acpi/tables/Fadt.h>
#include <acpi/tables/Tables.h>

Acpi_Status Acpi_FadtParse(const Acpi_Fadt *fadt, Acpi_FadtInfo *out)
{
    if (!fadt || !out)
        return kAcpiInvalidArg;

    out->Flags      = fadt->Flags;
    out->BootArch   = fadt->IapcBootArch;
    out->HwReduced  = (fadt->Flags & ACPI_FADT_HW_REDUCED) != 0;
    out->HasCmosRtc = !(fadt->IapcBootArch & ACPI_FADT_CMOS_RTC_GONE);
    out->Has8042    = (fadt->IapcBootArch & ACPI_FADT_LEGACY_DEVS) != 0;
    out->SciGsi     = fadt->SciInterrupt;
    out->PmProfile  = fadt->PreferredPmProfile;

    out->DsdtAddress = (fadt->Header.Length >= 148 && fadt->XDsdt != 0) ? fadt->XDsdt : fadt->Dsdt;

    out->FacsAddress =
        (fadt->Header.Length >= 140 && fadt->XFirmwareCtrl != 0) ? fadt->XFirmwareCtrl : fadt->FirmwareCtrl;

    Acpi_OsLog(kAcpiLogInfo, "FADT: %s mode, SCI GSI %u, PM profile %u", out->HwReduced ? "HW-reduced" : "full",
               out->SciGsi, out->PmProfile);

    return kAcpiOk;
}

const Acpi_Fadt *Acpi_FadtGet(void)
{
    return (const Acpi_Fadt *)Acpi_FindTable(Acpi_SigFADT, 0);
}
