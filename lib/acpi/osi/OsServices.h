#ifndef LIB_ACPI_OSI_OS_SERVICES_H
#define LIB_ACPI_OSI_OS_SERVICES_H

#include <acpi/AcpiTypes.h>

typedef enum : unsigned char
{
    kAcpiLogError,
    kAcpiLogWarn,
    kAcpiLogInfo,
    kAcpiLogDebug,
    kAcpiLogTrace,
} Acpi_LogLevel;

[[gnu::format(printf, 2, 3)]] void Acpi_OsLog(Acpi_LogLevel level, const char *fmt, ...);

Acpi_Vaddr Acpi_OsMap(Acpi_Paddr phys, Acpi_Usize length);
void       Acpi_OsUnmap(Acpi_Vaddr virt, Acpi_Usize length);

void *Acpi_OsAllocate(Acpi_Usize size);
void  Acpi_OsFree(void *ptr);

Acpi_Status Acpi_OsPortRead(Acpi_Uint16 port, unsigned width, Acpi_Uint64 *value);
Acpi_Status Acpi_OsPortWrite(Acpi_Uint16 port, unsigned width, Acpi_Uint64 value);

Acpi_Status Acpi_OsMmioRead(Acpi_Vaddr addr, unsigned width, Acpi_Uint64 *value);
Acpi_Status Acpi_OsMmioWrite(Acpi_Vaddr addr, unsigned width, Acpi_Uint64 value);

Acpi_Status Acpi_OsPciRead(Acpi_Uint16 seg, Acpi_Uint8 bus, Acpi_Uint8 dev, Acpi_Uint8 func, Acpi_Uint16 offset,
                           unsigned width, Acpi_Uint64 *value);

Acpi_Status Acpi_OsPciWrite(Acpi_Uint16 seg, Acpi_Uint8 bus, Acpi_Uint8 dev, Acpi_Uint8 func, Acpi_Uint16 offset,
                            unsigned width, Acpi_Uint64 value);

void        Acpi_OsStall(Acpi_Uint64 us);
Acpi_Status Acpi_OsSleep(Acpi_Uint64 ms);

typedef struct Acpi_OsMutex *Acpi_OsMutex;

Acpi_Status Acpi_OsMutexCreate(Acpi_OsMutex *out);
void        Acpi_OsMutexDestroy(Acpi_OsMutex mtx);
Acpi_Status Acpi_OsMutexAcquire(Acpi_OsMutex mtx, Acpi_Uint64 timeoutMs);
void        Acpi_OsMutexRelease(Acpi_OsMutex mtx);

typedef bool (*Acpi_SciHandler)(void *ctx);

Acpi_Status Acpi_OsInstallSciHandler(Acpi_Uint32 gsi, Acpi_SciHandler handler, void *ctx);
void        Acpi_OsRemoveSciHandler(Acpi_Uint32 gsi, Acpi_SciHandler handler);

Acpi_Paddr Acpi_OsGetRsdpAddress(void);

#endif /* LIB_ACPI_OSI_OS_SERVICES_H */
