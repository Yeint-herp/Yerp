#ifndef PMM_HH
#define PMM_HH

#include <core/limine.hh>
#include <arch/idt.hh>
#include <ktl/type_traits>
#include <ktl/pair>
#include <ktl/assert>

#if 0

class PhysicalMemoryManager {
public:
    static constexpr u64 kMinPhysical = 0x0010'0000ULL;

    static constexpr u64 PageSize = 4096ULL;

    static void init() {
        if (!Limine::MemoryMap::available()) {
            InterruptDescriptorTable::kpanic(
                nullptr,
                "PhysicalMemoryManager: Limine memory map not available"
            );
        }

        if constexpr (kDebugMode) {
            Fmt::printf("PMM debug: Starting initialization\n");
        }

        auto count = Limine::MemoryMap::entryCount();
        if constexpr (kDebugMode) {
            Fmt::printf("PMM debug: Memory map contains {} entries\n", count);
        }

        for (usize i = 0; i < count; ++i) {
            auto* entry = Limine::MemoryMap::entryAt(i);
            if constexpr (kDebugMode) {
                Fmt::printf(
                    "PMM debug: Entry {}: base={:#x} length={:#x} type={}\n",
                    i,
                    static_cast<unsigned long long>(entry->base),
                    static_cast<unsigned long long>(entry->length),
                    entry->type
                );
            }

            if (entry->type == LIMINE_MEMMAP_USABLE) {
                u64 base   = entry->base;
                u64 length = entry->length;

                u64 region_start = base;
                u64 region_end   = base + length;

                if (region_end <= kMinPhysical) {
                    if constexpr (kDebugMode) {
                        Fmt::printf(
                            "PMM debug: Skipping region [{:#x}-{:#x}) entirely below kMinPhysical ({:#x})\n",
                            static_cast<unsigned long long>(region_start),
                            static_cast<unsigned long long>(region_end),
                            static_cast<unsigned long long>(kMinPhysical)
                        );
                    }
                    continue;
                }

                if (region_start < kMinPhysical) {
                    if constexpr (kDebugMode) {
                        Fmt::printf(
                            "PMM debug: Adjusting region start from {:#x} to kMinPhysical ({:#x})\n",
                            static_cast<unsigned long long>(region_start),
                            static_cast<unsigned long long>(kMinPhysical)
                        );
                    }
                    region_start = kMinPhysical;
                }

                if constexpr (kDebugMode) {
                    Fmt::printf(
                        "PMM debug: Adding region [{:#x}-{:#x}) (size {:#x})\n",
                        static_cast<unsigned long long>(region_start),
                        static_cast<unsigned long long>(region_end),
                        static_cast<unsigned long long>(region_end - region_start)
                    );
                }

                addRegion(region_start, region_end - region_start);
            } else {
                if constexpr (kDebugMode) {
                    Fmt::printf(
                        "PMM debug: Entry {} is not usable (type={}), skipping\n",
                        i,
                        entry->type
                    );
                }
            }
        }

        if constexpr (kDebugMode) {
            Fmt::printf(
                "PMM debug: Initialization complete. Total pages: {}, Free pages: {}\n",
                static_cast<unsigned long long>(s_total_pages),
                static_cast<unsigned long long>(s_free_pages)
            );
        }
    }

    static ktl::pair<u64, void*> allocate() {
        if (s_freelist_head == nullptr) {
            InterruptDescriptorTable::kpanic(
                nullptr,
                "PhysicalMemoryManager: Out of physical memory"
            );
        }

        FreePage* page = s_freelist_head;
        s_freelist_head = page->next;
        if constexpr (kPmmUseFifo) {
            if (s_freelist_head == nullptr) {
                s_freelist_tail = nullptr;
            }
        }

        --s_free_pages;

        u64 phys = reinterpret_cast<u64>(page);

        if constexpr (kPmmZeroOnAlloc) {
            volatile u8* p = reinterpret_cast<u8*>(phys);
            for (usize i = 0; i < PageSize; ++i) {
                p[i] = 0;
            }
        }

        void* virt = nullptr;
        if (Limine::HHDM::available()) {
            virt = reinterpret_cast<void*>(phys + Limine::HHDM::offset());
        }

        return { phys, virt };
    }

    static ktl::pair<u64, void*> tryAllocate() {
        if (s_freelist_head == nullptr) {
            return { 0, nullptr };
        }
        return allocate();
    }

    static void free(u64 phys_addr) {
        uptr virt = reinterpret_cast<uptr>(phys_addr + Limine::HHDM::offset());

        if ((phys_addr & (PageSize - 1)) != 0) {
            if constexpr (kPanicOnError) {
                InterruptDescriptorTable::kpanic(
                    nullptr,
                    "PhysicalMemoryManager::free: address {:#x} not 4 KiB-aligned",
                    phys_addr
                );
            } else {
                Fmt::printf(
                    "PMM warning: attempted to free non-aligned address {:#x}\n",
                    static_cast<unsigned long long>(phys_addr)
                );
                return;
            }
        }
        FreePage* page = reinterpret_cast<FreePage*>(virt);

        if constexpr (kDebugMode || kEnableDoubleFreeCheck) {
            for (FreePage* it = s_freelist_head; it != nullptr; it = it->next) {
                if (it == page) {
                    if constexpr (kPanicOnError) {
                        InterruptDescriptorTable::kpanic(
                            nullptr,
                            "PhysicalMemoryManager::free: Double-free detected for page {:#x}",
                            phys_addr
                        );
                    } else {
                        Fmt::printf(
                            "PMM warning: double-free detected at {:#x}\n",
                            static_cast<unsigned long long>(phys_addr)
                        );
                        return;
                    }
                }
            }
        }

        if constexpr (kPmmZeroOnFree) {
            volatile u8* p = reinterpret_cast<u8*>(virt);
            for (usize i = 0; i < PageSize; ++i) {
                p[i] = 0;
            }
        }

        if constexpr (kPmmUseFifo) {
            if (s_freelist_tail != nullptr) {
                s_freelist_tail->next = page;
                page->next = nullptr;
                s_freelist_tail = page;
            } else {
                page->next = nullptr;
                s_freelist_head = page;
                s_freelist_tail = page;
            }
        } else {
            page->next = s_freelist_head;
            s_freelist_head = page;
            if (s_freelist_tail == nullptr) {
                s_freelist_tail = page;
            }
        }

        ++s_free_pages;
    }

    static void reserveRegion(u64 base, u64 length) {
        u64 region_start = alignUp(base, PageSize);
        u64 region_end   = alignDown(base + length, PageSize);
        if (region_start >= region_end) {
            return;
        }

        for (u64 addr = region_start; addr < region_end; addr += PageSize) {
            FreePage* prev = nullptr;
            for (FreePage* it = s_freelist_head; it != nullptr; ) {
                if (reinterpret_cast<u64>(it) == addr) {
                    if (prev == nullptr) {

                        s_freelist_head = it->next;
                        it = s_freelist_head;
                    } else {
                        prev->next = it->next;
                        it = prev->next;
                    }
                    --s_free_pages;
                } else {
                    prev = it;
                    it = it->next;
                }
            }
        }

        if constexpr (kPmmUseFifo) {
            s_freelist_tail = nullptr;
            for (FreePage* it = s_freelist_head; it != nullptr; it = it->next) {
                if (it->next == nullptr) {
                    s_freelist_tail = it;
                    break;
                }
            }
        }
    }

    static u64 totalPages() {
        return s_total_pages;
    }

    static u64 freePages() {
        return s_free_pages;
    }

private:
    struct FreePage {
        FreePage* next;
        u32 flags;
    };

    static inline FreePage* s_freelist_head = nullptr;
    static inline FreePage* s_freelist_tail = nullptr;

    static inline u64 s_total_pages = 0;
    static inline u64 s_free_pages  = 0;

    static constexpr bool kEnableDoubleFreeCheck = true;

    static void addRegion(u64 base, u64 length) {
        u64 region_start = alignUp(base, PageSize);
        u64 region_end   = alignDown(base + length, PageSize);
        if (region_start >= region_end) {
            return;
        }

        for (u64 addr = region_start; addr < region_end; addr += PageSize) {
            FreePage* page = reinterpret_cast<FreePage*>(addr);
            page->next = nullptr;
            page->flags = 0;

            if constexpr (kPmmUseFifo) {
                if (s_freelist_tail != nullptr) {
                    s_freelist_tail->next = page;
                    s_freelist_tail = page;
                } else {
                    s_freelist_head = page;
                    s_freelist_tail = page;
                }
            } else {
                page->next = s_freelist_head;
                s_freelist_head = page;
                if (s_freelist_tail == nullptr) {
                    s_freelist_tail = page;
                }
            }

            ++s_total_pages;
            ++s_free_pages;
        }
    }

    static constexpr u64 alignUp(u64 addr, u64 align) {
        return (addr + (align - 1)) & ~(align - 1);
    }

    static constexpr u64 alignDown(u64 addr, u64 align) {
        return addr & ~(align - 1);
    }
};

#endif

#endif // PMM_HH
