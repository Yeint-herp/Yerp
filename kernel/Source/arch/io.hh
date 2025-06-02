#ifndef IO_H
#define IO_H

#include <ktl/assert>

class io {
public:
    template <typename T>
    static void out(u32 port, T value) {
        if constexpr (sizeof(T) == 1) {
            __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
        } else if constexpr (sizeof(T) == 2) {
            __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
        } else if constexpr (sizeof(T) == 4) {
            __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
        } else if constexpr (sizeof(T) == 8) {
            __asm__ volatile ("outq %0, %1" : : "a"(value), "Nd"(port));
        } else {
            static_assert(sizeof(T) <= 8, "Invalid out operand size");
        }
    }

    template <typename T>
    static T in(u32 port) {
        T value{};
        if constexpr (sizeof(T) == 1) {
            __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
        } else if constexpr (sizeof(T) == 2) {
            __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
        } else if constexpr (sizeof(T) == 4) {
            __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
        } else if constexpr (sizeof(T) == 8) {
            __asm__ volatile ("inq %1, %0" : "=a"(value) : "Nd"(port));
        } else {
            static_assert(sizeof(T) <= 8, "Invalid in operand size");
        }
        return value;
    }

    static void cli() {
        __asm__ volatile ("cli");
    }

    static void sti() {
        __asm__ volatile ("sti");
    }

    static void hlt() {
        __asm__ volatile ("hlt");
    }

    class msr {
    public:
        static void write(u32 msr, u64 value) {
            u32 low = value & 0xFFFFFFFF;
            u32 high = value >> 32;
            __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
        }

        static u64 read(u32 msr) {
            u32 low, high;
            __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
            return (static_cast<u64>(high) << 32) | low;
        }
    };
};

#endif // IO_H
