#ifndef GDT_HH
#define GDT_HH

#include <core/format.hh>
#include <arch/serial.hh>
#include <ktl/string_view>

class GlobalDescriptorTable {
public:
    enum Segment : u16 {
        KERNEL_CODE = 0x08,
        KERNEL_DATA = 0x10,
        USER_CODE   = 0x18,
        USER_DATA   = 0x20,
        TSS         = 0x28,
    };

    static void load() {
        if constexpr (kDebugMode) {
            Fmt::printf("Loading GDT @ {:#016x}, size={} bytes\n",
                        gdt_ptr.base,
                        static_cast<u32>(gdt_ptr.size));
        }

        __asm__ volatile (
            "lgdt %[gdt]    \n\t"
            "mov $0x10, %%ax \n\t"
            "mov %%ax, %%ds \n\t"
            "mov %%ax, %%es \n\t"
            "mov %%ax, %%fs \n\t"
            "mov %%ax, %%gs \n\t"

            "pushq $0x08 \n\t"
            "lea 1f(%%rip), %%rax \n\t"
            "pushq %%rax \n\t"
            "lretq \n\t"
            "1:\n\t"

            "mov $0x28, %%ax \n\t"
            "ltr %%ax \n\t"
            :
            : [gdt] "m"(gdt_ptr)
            : "rax", "memory"
        );
    }

    void set_rsp0(const u64 rsp0) {
        if constexpr (kDebugMode) {
            FmtBase<SerialCOM2>::printf("GDT: setting TSS.rsp0 = {:#016x}\n", rsp0);
        }
        tss_instance.rsp0 = rsp0;
    }

    void set_ists(const u64 ists[7]) {
        if constexpr (kDebugMode) {
            FmtBase<SerialCOM2>::print("GDT: setting TSS.ists = {");
            for (int i = 0; i < 7; ++i) {
                FmtBase<SerialCOM2>::printf("{:#016x}", ists[i]);
                if (i + 1 < 7) {
                    FmtBase<SerialCOM2>::print(", ");
                }
            }
            FmtBase<SerialCOM2>::print("}\n");
        }

        __builtin_memcpy(tss_instance.ists, ists, sizeof(u64) * 7);
    }

private:
#pragma pack(push, 1)
    struct [[gnu::packed]] GDTEntry {
        u16 limit_low;
        u16 base_low;
        u8 base_mid;
        u8 access;
        u8 granularity;
        u8 base_high;
    };

    struct [[gnu::packed]] GDTEntry64 {
        GDTEntry base;
        u32 base_upper;
        u32 reserved = 0;
    };

    struct [[gnu::packed]] GDTPointer {
        u16 size;
        u64 base;
    };

    struct [[gnu::packed]] TSS64 {
        u32 reserved0;
        u64 rsp0;
        u64 rsp1;
        u64 rsp2;
        u64 reserved1;
        u64 ists[7];
        u64 reserved2;
        u16 reserved3;
        u16 io_map_base;
    };

    struct [[gnu::packed, gnu::aligned(16)]] GDTFullTable {
        GDTEntry entries[5];
        GDTEntry64 tss_entry;
    };
#pragma pack(pop)

    static constexpr u8 ACCESS_PRESENT     = 0b10010000;
    static constexpr u8 ACCESS_PRIV_KERNEL = 0b00000000;
    static constexpr u8 ACCESS_PRIV_USER   = 0b01100000;
    static constexpr u8 ACCESS_CODE        = 0b00001010;
    static constexpr u8 ACCESS_DATA        = 0b00000010;
    static constexpr u8 ACCESS_TSS         = 0b00001001;

    static constexpr u8 GRAN_4K    = 0b10000000;
    static constexpr u8 GRAN_32BIT = 0b01000000;
    static constexpr u8 GRAN_LONG  = 0b00100000;

    inline static TSS64 tss_instance = {
        .reserved0 = 0,
        .rsp0 = 0,
        .rsp1 = 0,
        .rsp2 = 0,
        .reserved1 = 0,
        .ists = {0},
        .reserved2 = 0,
        .reserved3 = 0,
        .io_map_base = sizeof(TSS64)
    };

    inline static GDTFullTable gdt_table = {
        .entries = {
            { 0, 0, 0, 0, 0, 0 },

            { 0xFFFF, 0x0000, 0x00, 0x9A, 0xA0, 0x00 },

            { 0xFFFF, 0x0000, 0x00, 0x92, 0xC0, 0x00 },

            { 0xFFFF, 0x0000, 0x00, 0xFA, 0xA0, 0x00 },

            { 0xFFFF, 0x0000, 0x00, 0xF2, 0xC0, 0x00 },
        },
        .tss_entry = {
            .base = {
                .limit_low = static_cast<u16>(sizeof(TSS64) - 1),
                .base_low = static_cast<u16>((u64)&tss_instance & 0xFFFF),
                .base_mid = static_cast<u8>(((u64)&tss_instance >> 16) & 0xFF),
                .access = 0x89,
                .granularity = static_cast<u8>((((u64)&tss_instance >> 16) & 0xF00) >> 8),
                .base_high = static_cast<u8>(((u64)&tss_instance >> 24) & 0xFF)
            },
            .base_upper = static_cast<u32>(((u64)&tss_instance) >> 32),
            .reserved = 0
        }
    };

    inline static GDTPointer gdt_ptr = {
        .size = static_cast<u16>(sizeof(gdt_table) - 1),
        .base = reinterpret_cast<u64>(&gdt_table)
    };

};

#endif //GDT_HH
