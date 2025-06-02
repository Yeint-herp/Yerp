#ifndef LIMINE_HH
#define LIMINE_HH

#if 0 // trying to debug memmap being zero, make the symbols not break stuff

#include <limine.h>

#include <ktl/optional>
#include <ktl/string_view>
#include <ktl/slice>
#include <ktl/type_traits>
#include <ktl/initializer_list>
#include <ktl/pair>
#include <ktl/tuple>

namespace Limine {

    class BootloaderInfo {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) && (request.response->revision != 0);
        }

        static constexpr ktl::string_view name() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->name, 
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(request.response->name)) };
        }

        static constexpr ktl::string_view version() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->version, 
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(request.response->version)) };
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_bootloader_info_request request = {
            .id       = LIMINE_BOOTLOADER_INFO_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class ExecutableCmdline {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) && (request.response->revision != 0);
        }

        static constexpr ktl::string_view cmdline() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->cmdline,
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(request.response->cmdline)) };
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_executable_cmdline_request request = {
            .id       = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class FirmwareType {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) && (request.response->revision != 0);
        }

        static constexpr u64 type() noexcept {
            if (!available()) {
                return static_cast<u64>(-1);
            }
            return request.response->firmware_type;
        }

        static constexpr ktl::string_view asString() noexcept {
            if (!available()) {
                return {};
            }
            switch (request.response->firmware_type) {
                case LIMINE_FIRMWARE_TYPE_X86BIOS: return "x86 BIOS";
                case LIMINE_FIRMWARE_TYPE_UEFI32:  return "UEFI (32-bit)";
                case LIMINE_FIRMWARE_TYPE_UEFI64:  return "UEFI (64-bit)";
                case LIMINE_FIRMWARE_TYPE_SBI:     return "SBI (RISC-V)";
                default:                          return "Unknown";
            }
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_firmware_type_request request = {
            .id       = LIMINE_FIRMWARE_TYPE_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class Framebuffer {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) &&
                (request.response->revision != 0) &&
                (request.response->framebuffer_count > 0);
        }

        static constexpr usize framebufferCount() noexcept {
            if (!available()) {
                return 0;
            }
            return static_cast<usize>(request.response->framebuffer_count);
        }

        static constexpr ktl::slice<limine_framebuffer*> framebuffers() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->framebuffers,
                    static_cast<ktl::slice<limine_framebuffer*>::size_type>(
                        request.response->framebuffer_count) };
        }

        struct FB {
            limine_framebuffer* fb;

            constexpr FB(limine_framebuffer* f) noexcept : fb(f) {}

            constexpr usize width()   const noexcept { return static_cast<usize>(fb->width); }
            constexpr usize height()  const noexcept { return static_cast<usize>(fb->height); }
            constexpr usize pitch()   const noexcept { return static_cast<usize>(fb->pitch); }
            constexpr u16 bpp()   const noexcept { return fb->bpp; }
            constexpr u8 model() const noexcept { return fb->memory_model; }
            constexpr void* address() const noexcept { return fb->address; }

            constexpr ktl::slice<unsigned char> asBytes() const noexcept {
                return { reinterpret_cast<unsigned char*>(fb->address),
                        static_cast<ktl::slice<unsigned char>::size_type>(
                            fb->height * fb->pitch) };
            }

            constexpr usize modeCount() const noexcept {
                return static_cast<usize>(fb->mode_count);
            }

            constexpr ktl::slice<limine_video_mode*> modes() const noexcept {
                return { fb->modes,
                        static_cast<ktl::slice<limine_video_mode*>::size_type>(
                            fb->mode_count) };
            }
        };

        static constexpr FB framebufferAt(usize idx) noexcept {
            if (idx >= framebufferCount()) {
                return nullptr;
            }
            return FB(request.response->framebuffers[idx]);
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_framebuffer_request request = {
            .id       = LIMINE_FRAMEBUFFER_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };
    
    class MemoryMap {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) &&
                (request.response->revision != 0) &&
                (request.response->entry_count > 0);
        }

        static constexpr usize entryCount() noexcept {
            if (!available()) {
                return 0;
            }
            return static_cast<usize>(request.response->entry_count);
        }

        static constexpr ktl::slice<limine_memmap_entry*> entries() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->entries,
                    static_cast<ktl::slice<limine_memmap_entry*>::size_type>(
                        request.response->entry_count) };
        }

        static constexpr limine_memmap_entry* entryAt(usize idx) noexcept {
            if (!available() || idx >= entryCount()) {
                return nullptr;
            }
            return request.response->entries[idx];
        }

    private: [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_memmap_request request = {
            .id = LIMINE_MEMMAP_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class ModuleList {
    public:
        static constexpr bool available() noexcept {
            return (request.response->revision != 0) && (request.response->module_count > 0);
        }

        static constexpr usize moduleCount() noexcept {
            if (!available()) {
                return 0;
            }
            return static_cast<usize>(request.response->module_count);
        }

        static constexpr ktl::slice<limine_file*> modules() noexcept {
            if (!available()) {
                return {};
            }
            return { request.response->modules,
                    static_cast<ktl::slice<limine_file*>::size_type>(
                        request.response->module_count) };
        }

        static constexpr limine_file* moduleAt(usize idx) noexcept {
            if (idx >= moduleCount()) {
                return nullptr;
            }
            return request.response->modules[idx];
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_module_request request = {
            .id                  = LIMINE_MODULE_REQUEST,
            .revision            = LIMINE_API_REVISION,
            .response            = nullptr,
            .internal_module_count = 0,
            .internal_modules    = nullptr
        };
    };

    class ExecutableFileInfo {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) &&
                (request.response->revision != 0) &&
                (file() != nullptr);
        }

        static constexpr limine_file* file() noexcept {
    #if LIMINE_API_REVISION >= 2
            return request.response->executable_file;
    #else
            return request.response->kernel_file;
    #endif
        }

        static constexpr ktl::string_view path() noexcept {
            if (!available()) {
                return {};
            }
            return { file()->path,
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(file()->path)) };
        }

    #if LIMINE_API_REVISION >= 3
        static constexpr ktl::string_view string() noexcept {
            if (!available()) {
                return {};
            }
    #if LIMINE_API_REVISION >= 3
            return { file()->string,
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(file()->string)) };
    #else
            return {};
    #endif
        }
    #else
        static constexpr ktl::string_view cmdline() noexcept {
            if constexpr (!available()) {
                return {};
            }
    #if LIMINE_API_REVISION < 3
            return { file()->cmdline,
                    static_cast<ktl::string_view::size_type>(
                        __builtin_strlen(file()->cmdline)) };
    #else
            return {};
    #endif
        }
    #endif
    private:
    #if LIMINE_API_REVISION >= 2
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_executable_file_request  request  = {
            .id       = LIMINE_EXECUTABLE_FILE_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #else
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_kernel_file_request  request  = {
            .id       = LIMINE_KERNEL_FILE_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #endif
    };

    class ExecutableAddressInfo {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) &&
                (request.response->revision != 0);
        }

        static constexpr u64 physicalBase() noexcept {
            if (!available()) {
                return 0;
            }
    #if LIMINE_API_REVISION >= 2
            return request.response->physical_base;
    #else
            return request.response->physical_base;
    #endif
        }

        static constexpr u64 virtualBase() noexcept {
            if (!available()) {
                return 0;
            }
    #if LIMINE_API_REVISION >= 2
            return request.response->virtual_base;
    #else
            return request.response->virtual_base;
    #endif
        }

    private:
    #if LIMINE_API_REVISION >= 2
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_executable_address_request  request  = {
            .id       = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #else
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_kernel_address_request  request  = {
            .id       = LIMINE_KERNEL_ADDRESS_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #endif
    };

    class EFISupport {
    public:
        static constexpr bool systemTableAvailable() noexcept {
            return (sysRequest.response->revision != 0);
        }

        static constexpr u64 efiSystemTableAddress() noexcept {
            return sysRequest.response->address;
        }

        static constexpr bool memmapAvailable() noexcept {
            return (memRequest.response->revision != 0);
        }

        static constexpr void* efiMemmapAddress() noexcept {
            return memRequest.response->memmap;
        }
        static constexpr u64 efiMemmapSize() noexcept {
            return memRequest.response->memmap_size;
        }
        static constexpr u64 efiDescSize() noexcept {
            return memRequest.response->desc_size;
        }
        static constexpr u64 efiDescVersion() noexcept {
            return memRequest.response->desc_version;
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_efi_system_table_request  sysRequest  = {
            .id       = LIMINE_EFI_SYSTEM_TABLE_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };

        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_efi_memmap_request  memRequest  = {
            .id       = LIMINE_EFI_MEMMAP_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class RSDPInfo {
    public:
        static constexpr bool available() noexcept {
            return (request.response->revision != 0);
        }

        static constexpr u64 address() noexcept {
            return request.response->address;
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_rsdp_request  request  = {
            .id       = LIMINE_RSDP_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };
    
    class SMBIOSInfo {
    public:
        static constexpr bool available() noexcept {
            return (request.response->revision != 0);
        }

        static constexpr u64 entry32() noexcept {
            return request.response->entry_32;
        }
        static constexpr u64 entry64() noexcept {
            return request.response->entry_64;
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_smbios_request  request  = {
            .id       = LIMINE_SMBIOS_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

    class BootTime {
    public:
        static constexpr bool available() noexcept {
            return (request.response->revision != 0);
        }

        static constexpr int64_t timestamp() noexcept {
    #if LIMINE_API_REVISION >= 3
            return request.response->timestamp;
    #else
            return request.response->boot_time;
    #endif
        }

    private:
    #if LIMINE_API_REVISION >= 3
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_date_at_boot_request  request  = {
            .id       = LIMINE_DATE_AT_BOOT_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #else
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_boot_time_request  request  = {
            .id       = LIMINE_BOOT_TIME_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    #endif
    };

    class HHDM {
    public:
        static constexpr bool available() noexcept {
            return (request.response != nullptr) && (request.response->revision != 0);
        }

        static constexpr u64 offset() noexcept {
            if (!available()) {
                return 0;
            }
            return request.response->offset;
        }

    private:
        [[gnu::used, gnu::section(".limine_reqs")]] volatile static inline limine_hhdm_request request = {
            .id       = LIMINE_HHDM_REQUEST,
            .revision = LIMINE_API_REVISION,
            .response = nullptr
        };
    };

} // namespace Limine

#endif

#endif //LIMINE_HH
