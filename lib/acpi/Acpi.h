#ifndef LIB_ACPI_ACPI_H
#define LIB_ACPI_ACPI_H

#include <acpi/AcpiTypes.h>
#include <acpi/tables/Fadt.h>
#include <acpi/tables/Tables.h>

#define Acpi__MakeSig(a, b, c, d)                                                                                      \
    ((Acpi_Uint32)(a) | ((Acpi_Uint32)(b) << 8) | ((Acpi_Uint32)(c) << 16) | ((Acpi_Uint32)(d) << 24))

#define Acpi_SigRSDP Acpi__MakeSig('R', 'S', 'D', 'P')
#define Acpi_SigRSDT Acpi__MakeSig('R', 'S', 'D', 'T')
#define Acpi_SigXSDT Acpi__MakeSig('X', 'S', 'D', 'T')
#define Acpi_SigFADT Acpi__MakeSig('F', 'A', 'C', 'P')
#define Acpi_SigMADT Acpi__MakeSig('A', 'P', 'I', 'C')
#define Acpi_SigMCFG Acpi__MakeSig('M', 'C', 'F', 'G')
#define Acpi_SigHPET Acpi__MakeSig('H', 'P', 'E', 'T')
#define Acpi_SigSRAT Acpi__MakeSig('S', 'R', 'A', 'T')
#define Acpi_SigSLIT Acpi__MakeSig('S', 'L', 'I', 'T')
#define Acpi_SigDSDT Acpi__MakeSig('D', 'S', 'D', 'T')
#define Acpi_SigSSDT Acpi__MakeSig('S', 'S', 'D', 'T')
#define Acpi_SigFACS Acpi__MakeSig('F', 'A', 'C', 'S')
#define Acpi_SigBGRT Acpi__MakeSig('B', 'G', 'R', 'T')
#define Acpi_SigGTDT Acpi__MakeSig('G', 'T', 'D', 'T')
#define Acpi_SigIORT Acpi__MakeSig('I', 'O', 'R', 'T')
#define Acpi_SigPPTT Acpi__MakeSig('P', 'P', 'T', 'T')

const Acpi_SdtHeader *Acpi_FindTable(Acpi_Uint32 signature, Acpi_Usize index);
const Acpi_FadtInfo  *Acpi_GetFadtInfo(void);

/// static table initialization.
Acpi_Status Acpi_EarlyInit(void);

/// full aml initialization.
Acpi_Status Acpi_Init(void);

#endif /* LIB_ACPI_ACPI_H */
