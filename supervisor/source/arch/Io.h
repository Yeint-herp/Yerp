#ifndef SUPERVISOR_ARCH_IO_H
#define SUPERVISOR_ARCH_IO_H

u8   Arch_IoIn8(u16 port);
void Arch_IoOut8(u16 port, u8 data);

u16  Arch_IoIn16(u16 port);
void Arch_IoOut16(u16 port, u16 data);

u32  Arch_IoIn32(u16 port);
void Arch_IoOut32(u16 port, u32 data);

void Arch_IoWait(void);

u8   Arch_MmioRead8(const void *addr);
void Arch_MmioWrite8(void *addr, u8 data);

u16  Arch_MmioRead16(const void *addr);
void Arch_MmioWrite16(void *addr, u16 data);

u32  Arch_MmioRead32(const void *addr);
void Arch_MmioWrite32(void *addr, u32 data);

u64  Arch_MmioRead64(const void *addr);
void Arch_MmioWrite64(void *addr, u64 data);

#endif /* SUPERVISOR_ARCH_IO_H */
