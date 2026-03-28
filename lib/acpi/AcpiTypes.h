#ifndef LIB_ACPI_ACPI_TYPES_H
#define LIB_ACPI_ACPI_TYPES_H

typedef unsigned char      Acpi_Uint8;
typedef unsigned short     Acpi_Uint16;
typedef unsigned int       Acpi_Uint32;
typedef unsigned long long Acpi_Uint64;

typedef signed char      Acpi_Int8;
typedef signed short     Acpi_Int16;
typedef signed int       Acpi_Int32;
typedef signed long long Acpi_Int64;

typedef typeof(sizeof(Acpi_Uint8)) Acpi_Usize;
typedef Acpi_Usize                 Acpi_Uptr;
typedef signed long                Acpi_Isize;
typedef Acpi_Isize                 Acpi_Iptr;

typedef Acpi_Uptr Acpi_Paddr;
typedef void     *Acpi_Vaddr;

typedef enum : Acpi_Int32
{
    kAcpiOk             = 0,
    kAcpiError          = -1,
    kAcpiNoMemory       = -2,
    kAcpiNotFound       = -3,
    kAcpiInvalidArg     = -4,
    kAcpiBadChecksum    = -5,
    kAcpiBadSignature   = -6,
    kAcpiUnsupported    = -7,
    kAcpiTimeout        = -8,
    kAcpiAlreadyExists  = -9,
    kAcpiBadHeader      = -10,
    kAcpiBufferTooSmall = -11,
} Acpi_Status;

#define Acpi_Success(s)                                                                                                \
    ({                                                                                                                 \
        Acpi_Status __s = (s);                                                                                         \
        __s >= 0;                                                                                                      \
    })

#define Acpi_Failure(s)                                                                                                \
    ({                                                                                                                 \
        Acpi_Status __s = (s);                                                                                         \
        __s < 0;                                                                                                       \
    })

#define Acpi_Min(a, b)                                                                                                 \
    ({                                                                                                                 \
        typeof(a) _a = (a);                                                                                            \
        typeof(b) _b = (b);                                                                                            \
        _a < _b ? _a : _b;                                                                                             \
    })

#define Acpi_Max(a, b)                                                                                                 \
    ({                                                                                                                 \
        typeof(a) _a = (a);                                                                                            \
        typeof(b) _b = (b);                                                                                            \
        _a > _b ? _a : _b;                                                                                             \
    })

#endif /* LIB_ACPI_ACPI_TYPES_H */
