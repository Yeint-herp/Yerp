#ifndef SUPERVISOR_IO_DEVICE_H
#define SUPERVISOR_IO_DEVICE_H

#include <core/Spinlock.h>
#include <dispatcher/Dispatcher.h>
#include <dsa/List.h>
#include <executive/Object.h>
#include <io/Irp.h>

struct Io_Driver;

#define IO_DEVICE_BUFFERED_IO  (1u << 0)
#define IO_DEVICE_DIRECT_IO    (1u << 1)
#define IO_DEVICE_EXCLUSIVE    (1u << 2)
#define IO_DEVICE_INITIALIZING (1u << 3)

enum : i32
{
    IO_DEVICE_TYPE_UNKNOWN    = 0,
    IO_DEVICE_TYPE_DISK       = 1,
    IO_DEVICE_TYPE_SERIAL     = 2,
    IO_DEVICE_TYPE_KEYBOARD   = 3,
    IO_DEVICE_TYPE_DISPLAY    = 4,
    IO_DEVICE_TYPE_NETWORK    = 5,
    IO_DEVICE_TYPE_BUS        = 6,
    IO_DEVICE_TYPE_CONTROLLER = 7,
};

typedef struct Io_DeviceQueue
{
    Dsa_ListEntry Head;
    Core_Spinlock Lock;
    bool          Busy;
} Io_DeviceQueue;

typedef struct Io_Device
{
    struct Io_Driver *Driver;

    struct Io_Device *AttachedDevice;
    struct Io_Device *LowerDevice;

    Dsa_ListEntry DriverLink;

    u32 DeviceType;
    u32 Flags;
    u32 AlignmentRequirement;
    u32 StackSize;

    Io_DeviceQueue Queue;

    void *DeviceExtension;
    usize DeviceExtensionSize;

    Core_Spinlock Lock;
    u32           ReferenceCount;

    char *DeviceName;
} Io_Device;

i32 Io_CreateDevice(struct Io_Driver *driver, usize extensionSize, const char *name, u32 deviceType, u32 flags,
                    Io_Device **outDevice);

void     Io_DeleteDevice(Io_Device *device);
Ob_Type *Io_GetDeviceType(void);

void *Io_GetDeviceExtension(Io_Device *device);

Io_Device *Io_AttachDevice(Io_Device *sourceDevice, Io_Device *targetDevice);
void       Io_DetachDevice(Io_Device *targetDevice);
Io_Device *Io_GetTopDevice(Io_Device *device);

void Io_DeviceQueueInit(Io_DeviceQueue *queue);
void Io_StartPacket(Io_Device *device, Io_Irp *irp);
void Io_StartNextPacket(Io_Device *device);

i32 Io_CallDriver(Io_Device *device, Io_Irp *irp);

#endif /* SUPERVISOR_IO_DEVICE_H */
