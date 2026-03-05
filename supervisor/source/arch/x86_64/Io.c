#include <arch/Io.h>

u8 Arch_IoIn8(u16 port)
{
    u8 data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "Nd"(port) : "memory");
    return data;
}

void Arch_IoOut8(u16 port, u8 data)
{
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port) : "memory");
}

u16 Arch_IoIn16(u16 port)
{
    u16 data;
    __asm__ volatile("inw %1, %0" : "=a"(data) : "Nd"(port) : "memory");
    return data;
}

void Arch_IoOut16(u16 port, u16 data)
{
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port) : "memory");
}

u32 Arch_IoIn32(u16 port)
{
    u32 data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(port) : "memory");
    return data;
}

void Arch_IoOut32(u16 port, u32 data)
{
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port) : "memory");
}

void Arch_IoWait(void)
{
    Arch_IoOut8(0x80, 0);
}

u8 Arch_MmioRead8(const void *addr)
{
    return *(const volatile u8 *)addr;
}

void Arch_MmioWrite8(void *addr, u8 data)
{
    *(volatile u8 *)addr = data;
}

u16 Arch_MmioRead16(const void *addr)
{
    return *(const volatile u16 *)addr;
}

void Arch_MmioWrite16(void *addr, u16 data)
{
    *(volatile u16 *)addr = data;
}

u32 Arch_MmioRead32(const void *addr)
{
    return *(const volatile u32 *)addr;
}

void Arch_MmioWrite32(void *addr, u32 data)
{
    *(volatile u32 *)addr = data;
}

u64 Arch_MmioRead64(const void *addr)
{
    return *(const volatile u64 *)addr;
}

void Arch_MmioWrite64(void *addr, u64 data)
{
    *(volatile u64 *)addr = data;
}
