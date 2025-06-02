#include <limine.h>
#include <arch/io.hh>
#include <arch/serial.hh>
#include <core/format.hh>

#include <arch/gdt.hh>
#include <arch/idt.hh>
#include <core/pmm.hh>

#include <arch/efi.hh>

[[gnu::used, gnu::section(".limine_requests")]] static volatile LIMINE_BASE_REVISION(3);

[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile LIMINE_REQUESTS_START_MARKER;

[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile LIMINE_REQUESTS_END_MARKER;

[[gnu::used, gnu::section(".limine_requests")]] volatile limine_memmap_request request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = LIMINE_API_REVISION,
    .response = nullptr
};

extern "C" void _start(void) {
    SerialCOM1::init();
    SerialCOM2::init();
    FmtBase<SerialCOM2>::print("\n ----------- \n");

    if (request.response == nullptr) Fmt::printf("Memmap is still null...\n");

//     if (LIMINE_BASE_REVISION_SUPPORTED == false) {
//         Fmt::printf("Base features not supported by limine, please update your bootloader!\n");
//         goto hcf;
//     }

//     GlobalDescriptorTable::load();
//     InterruptDescriptorTable::init();
//     PhysicalMemoryManager::init();

// hcf:
    io::cli();
    for(;;) io::hlt();
}
