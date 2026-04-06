#ifndef SUPERVISOR_IO_DRIVER_H
#define SUPERVISOR_IO_DRIVER_H

#include <executive/Object.h>
#include <io/Irp.h>

struct Io_Device;
struct Io_Driver;

typedef i32 (*Io_DispatchRoutine)(struct Io_Device *device, Io_Irp *irp);

typedef i32 (*Io_DriverEntry)(struct Io_Driver *driver);
typedef void (*Io_DriverUnload)(struct Io_Driver *driver);

typedef i32 (*Io_AddDeviceRoutine)(struct Io_Driver *driver, struct Io_Device *physicalDevice);

typedef struct Io_Driver
{
    const char         *Name;
    Io_DriverEntry      DriverEntry;
    Io_DriverUnload     DriverUnload;
    Io_AddDeviceRoutine AddDevice;

    Io_DispatchRoutine DispatchTable[IRP_MJ_COUNT];

    Dsa_ListEntry DeviceListHead;
    Core_Spinlock DeviceListLock;
    u32           DeviceCount;

    void *DriverExtension;
    usize DriverExtensionSize;

    u32 Flags;
} Io_Driver;

i32      Io_CreateDriver(const char *name, Io_DriverEntry entry, usize extensionSize, Io_Driver **outDriver);
void     Io_DeleteDriver(Io_Driver *driver);
Ob_Type *Io_GetDriverType(void);

void *Io_GetDriverExtension(Io_Driver *driver);

#endif /* SUPERVISOR_IO_DRIVER_H */
