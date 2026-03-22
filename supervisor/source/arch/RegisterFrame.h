#ifndef SUPERVISOR_ARCH_REGISTER_FRAME_H
#define SUPERVISOR_ARCH_REGISTER_FRAME_H

#if kArch == x86_64
#include <arch/x86_64/RegisterFrame.h>
#else
#error "Invalid architecture"
#endif

#endif /* SUPERVISOR_ARCH_REGISTER_FRAME_H */
