#ifndef SUPERVISOR_IO_IOCONTROL_H
#define SUPERVISOR_IO_IOCONTROL_H

#define IO_METHOD_BUFFERED   0
#define IO_METHOD_IN_DIRECT  1
#define IO_METHOD_OUT_DIRECT 2
#define IO_METHOD_NEITHER    3

#define IO_ACCESS_ANY   0
#define IO_ACCESS_READ  1
#define IO_ACCESS_WRITE 2
#define IO_ACCESS_RW    3

#define IO_CTL_CODE(devType, function, method, access)                                                                 \
    ((u32)(((devType) << 16) | ((method) << 14) | ((function) << 2) | (access)))

#define IO_CTL_DEVICE_TYPE(code) (((code) >> 16) & 0xFFFFu)
#define IO_CTL_METHOD(code)      (((code) >> 14) & 0x3u)
#define IO_CTL_FUNCTION(code)    (((code) >> 2) & 0xFFFu)
#define IO_CTL_ACCESS(code)      ((code) & 0x3u)

#endif /* SUPERVISOR_IO_IOCONTROL_H */
