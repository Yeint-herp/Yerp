#ifndef SUPERVISOR_ARCH_SPCR_H
#define SUPERVISOR_ARCH_SPCR_H

#if kArch == x86_64
#include <arch/x86_64/Spcr.h>
#else
#error "Invalid architecture"
#endif

#endif /* SUPERVISOR_ARCH_SPCR_H */
