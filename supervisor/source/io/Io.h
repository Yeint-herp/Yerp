#ifndef SUPERVISOR_IO_IO_H
#define SUPERVISOR_IO_IO_H

#include <executive/Pool.h>

#define IO_TAG_IRP    EX_TAG('I', 'o', 'R', 'p')
#define IO_TAG_DRIVER EX_TAG('I', 'o', 'D', 'r')
#define IO_TAG_DEVICE EX_TAG('I', 'o', 'D', 'v')
#define IO_TAG_FILE   EX_TAG('I', 'o', 'F', 'l')
#define IO_TAG_MISC   EX_TAG('I', 'o', 'M', 's')

enum
{
    kIoSuccess,
    kIoInvalidParameter,
    kIoInsufficientResources,
    kIoNotFound,
    kIoAccessDenied,
    kIoDeviceError,
    kIoPending,
    kIoCancelled,
    kIoEndOfFile,
    kIoNotSupported,
    kIoTimeout,
    kIoBufferTooSmall,
    kIoMediaChanged,
    kIoDeviceNotReady,
};

void Io_SystemInit(void);

#endif /* SUPERVISOR_IO_IO_H */
