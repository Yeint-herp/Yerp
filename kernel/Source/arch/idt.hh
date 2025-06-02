#ifndef IDT_HH
#define IDT_HH

#include <core/format.hh>
#include <arch/serial.hh>
#include <arch/io.hh>
#include <ktl/string_view>

struct [[gnu::packed]] registers_ctx {
    u64 es, ds;
    u64 cr4, cr3, cr2, cr0;
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rax, rbx, rcx, rdx, rsi, rdi;
    u64 rbp;
    u64 interrupt_vector, error_code;
    u64 rip, cs, rflags, rsp, ss;
};

extern "C" {
    extern uintptr_t isr_stub_table[];  
    extern void (*real_handler_table[])(registers_ctx*);
}

class InterruptDescriptorTable {
public:
    using Handler = void(*)(registers_ctx*);

    static constexpr u8 INTERRUPT_GATE = 0b1000'1110;
    static constexpr u8 TRAP_GATE      = 0b1000'1111;

    static void init() {
        idt_ptr.limit = static_cast<u16>(sizeof(idt_table) - 1);
        idt_ptr.base  = reinterpret_cast<u64>(&idt_table);

        if constexpr (kDebugMode) {
            Fmt::printf("Loading IDT @ {:#016x}, limit={} bytes\n",
                        idt_ptr.base, idt_ptr.limit);
        }

        constexpr u8 exceptionGate =
            (kIdtUseTrapGateForExceptions ? TRAP_GATE
                                          : INTERRUPT_GATE);

        for (usize vec = 0; vec < 32; ++vec) {
            setGate(vec, isr_stub_table[vec], exceptionGate);
            if constexpr (kDebugMode) {
                FmtBase<SerialCOM2>::print("IDT: vector ");
                FmtBase<SerialCOM2>::printf("{:#02x} -> default handler (exception)\n",
                                   static_cast<u32>(vec));
            }

            if constexpr (!kIdtPanicOnException) {
                real_handler_table[vec] = haltCatchFire;
            } else {
                real_handler_table[vec] = defaultInterruptHandler;
            }
        }

        for (usize vec = 32; vec < 256; ++vec) {
            setGate(vec, isr_stub_table[vec], INTERRUPT_GATE);
            if constexpr (kDebugMode) {
                FmtBase<SerialCOM2>::print("IDT: vector ");
                FmtBase<SerialCOM2>::printf("{:#02x} -> default handler (interrupt)\n",
                                   static_cast<u32>(vec));
            }
            real_handler_table[vec] = defaultInterruptHandler;
        }

        load();
    }

    [[noreturn]] static void kpanic(registers_ctx* ctx, ktl::string_view fmt, auto&&... args) {
        using Out = Fmt;
        using Log = FmtBase<SerialCOM2>;

        #define ANSI_RED     "\x1b[31m"
        #define ANSI_BOLD    "\x1b[1m"
        #define ANSI_RESET   "\x1b[0m"

        Out::print(ANSI_RED);
        Out::print(ANSI_BOLD);
        Out::print("\n!!! KERNEL PANIC !!!\n");
        Out::print(ANSI_RESET);

        #undef ANSI_RED
        #undef ANSI_BOLD
        #undef ANSI_RESET

        Out::printf(fmt.data(), ktl::forward<decltype(args)>(args)...);
        Out::print("\nSystem halted.\n");

        Log::print("======== KERNEL PANIC ========\n");
        Log::printf(fmt.data(), ktl::forward<decltype(args)>(args)...);
        Log::print("\n\n");

        if (ctx) {
            Log::print("Register dump:\n");
            Log::printf("RAX={:#016x} RBX={:#016x} RCX={:#016x} RDX={:#016x}\n",
                        ctx->rax, ctx->rbx,
                        ctx->rcx, ctx->rdx);
            Log::printf("RSI={:#016x} RDI={:#016x} RBP={:#016x} RSP={:#016x}\n",
                        ctx->rsi, ctx->rdi,
                        ctx->rbp, ctx->rsp);
            Log::printf("R8 ={:#016x} R9 ={:#016x} R10={:#016x} R11={:#016x}\n",
                        ctx->r8, ctx->r9,
                        ctx->r10, ctx->r11);
            Log::printf("R12={:#016x} R13={:#016x} R14={:#016x} R15={:#016x}\n",
                        ctx->r12, ctx->r13,
                        ctx->r14, ctx->r15);
            Log::printf("RIP={:#016x} RFLAGS={:#016x}\n",
                        ctx->rip, ctx->rflags);
            Log::printf("CS ={:#04x} SS={:#04x}\n",
                        ctx->cs, ctx->ss);
        }

        Log::print("\nSystem halted.\n");

        haltCatchFire(ctx);
    }

    static bool registerHandler(u16 vector, Handler h) {
        if (real_handler_table[vector] == defaultInterruptHandler ||
            real_handler_table[vector] == haltCatchFire)
        {
            real_handler_table[vector] = h;
            if constexpr (kDebugMode) {
                FmtBase<SerialCOM2>::print("IDT: custom handler registered for vector ");
                FmtBase<SerialCOM2>::printf("{:#02x}\n", vector);
            }
            return true;
        }
        return false;
    }

private:
    [[noreturn]] static void defaultInterruptHandler(registers_ctx* ctx) {
        const auto vector = ctx->interrupt_vector;
        if (vector < 32) {
            kpanic(ctx, "Unhandled CPU Exception {:#02x}: {}", 
                vector, exception_names[vector].data());
        } else {
            kpanic(ctx, "Unhandled Interrupt Vector {:#02x}", vector);
        }
    }

    [[noreturn]] static void haltCatchFire(registers_ctx*) {
        // TODO halt other cores
        io::cli();
        for (;;) io::hlt();
        __builtin_unreachable();
    }

    static inline void load() {
        __asm__ volatile("lidt %0" : : "m"(idt_ptr) : "memory");
    }

    static inline void setGate(usize vec, uintptr_t base, u8 flags) {
        auto& e = idt_table[vec];
        e.offset_low  = u16(base & 0xFFFF);
        e.selector    = 0x08;
        e.ist         = 0;
        e.attribute   = flags;
        e.offset_mid  = u16((base >> 16) & 0xFFFF);
        e.offset_high = u32((base >> 32) & 0xFFFFFFFF);
        e.zero        = 0;
    }

    struct [[gnu::packed]] Entry {
        u16 offset_low;
        u16 selector;
        u8  ist;
        u8  attribute;
        u16 offset_mid;
        u32 offset_high;
        u32 zero;
    };

    struct [[gnu::packed]] Ptr {
        u16 limit;
        u64 base;
    };

    static inline Entry  idt_table[256] = {};
    static inline Ptr    idt_ptr        = {};

    static inline ktl::string_view exception_names[32] = {
        "Divide-by-zero Error",        // 0
        "Debug",                       // 1
        "Non-maskable Interrupt",      // 2
        "Breakpoint",                  // 3
        "Overflow",                    // 4
        "Bound Range Exceeded",        // 5
        "Invalid Opcode",              // 6
        "Device Not Available",        // 7
        "Double Fault",                // 8
        "Coprocessor Segment Overrun", // 9
        "Invalid TSS",                 // 10
        "Segment Not Present",         // 11
        "Stack Segment Fault",         // 12
        "General Protection Fault",    // 13
        "Page Fault",                  // 14
        "Reserved",                    // 15
        "x87 Floating-Point Exception",// 16
        "Alignment Check",             // 17
        "Machine Check",               // 18
        "SIMD Floating-Point Exception", // 19
        "Virtualization Exception",    // 20
        "Control Protection Exception",// 21
        "Reserved",                    // 22
        "Reserved",                    // 23
        "Reserved",                    // 24
        "Reserved",                    // 25
        "Reserved",                    // 26
        "Reserved",                    // 27
        "Hypervisor Injection Exception", // 28
        "VMM Communication Exception", // 29
        "Security Exception",          // 30
        "Reserved"                     // 31
    };

    static_assert(sizeof(InterruptDescriptorTable::Entry) == 16,
                "IDT entry must be 16 bytes");
    static_assert(sizeof(registers_ctx) % 16 == 0,
                "registers_ctx must be 16-byte aligned");
};

#endif //IDT_HH
