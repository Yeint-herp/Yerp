#include <boot/Limine.h>
#include <limine.h>

[[gnu::used, gnu::section(".limine_requests")]]
volatile u64 Boot_LimineBaseRevision[] = LIMINE_BASE_REVISION(5);

[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile u64 s_LimineStartMarker[] = LIMINE_REQUESTS_START_MARKER;
[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile u64 s_LimineEndMarker[] = LIMINE_REQUESTS_END_MARKER;

[[gnu::used, gnu::section(".limine_requests")]]
volatile struct limine_memmap_request Boot_LimineMemmapReq = {.id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
volatile struct limine_hhdm_request Boot_LimineHhdmReq = {.id = LIMINE_HHDM_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
volatile struct limine_executable_address_request Boot_LimineExecAddrReq = {.id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
                                                                            .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
volatile struct limine_mp_request Boot_LimineSmpReq = {.id = LIMINE_MP_REQUEST_ID, .revision = 0};

[[gnu::used, gnu::section(".limine_requests")]]
volatile struct limine_rsdp_request Boot_Limine_RsdpReq = {.id = LIMINE_RSDP_REQUEST_ID, .revision = 0};
