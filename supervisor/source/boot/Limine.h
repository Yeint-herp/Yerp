#ifndef SUPERVISOR_BOOT_LIMINE_H
#define SUPERVISOR_BOOT_LIMINE_H

#include <limine.h>

extern volatile u64 Boot_LimineBaseRevision[];

extern volatile struct limine_memmap_request             Boot_LimineMemmapReq;
extern volatile struct limine_hhdm_request               Boot_LimineHhdmReq;
extern volatile struct limine_executable_address_request Boot_LimineExecAddrReq;
extern volatile struct limine_mp_request                 Boot_LimineSmpReq;
extern volatile struct limine_rsdp_request               Boot_Limine_RsdpReq;

#endif /* SUPERVISOR_BOOT_LIMINE_H */
