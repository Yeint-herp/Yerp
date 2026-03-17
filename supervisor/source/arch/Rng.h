#ifndef SUPERVISOR_ARCH_RNG_H
#define SUPERVISOR_ARCH_RNG_H

typedef enum
{
    RNG_SRC_NONE = 0,
    RNG_SRC_RDSEED,
    RNG_SRC_RDRAND,
    RNG_SRC_TSC,
} Arch_RngSource;

void Arch_RngInit(void);

Arch_RngSource Arch_RngGetSource(void);

bool Arch_RngFill(void *buf, usize len);
u64  Arch_RngU64(void);

#endif /* SUPERVISOR_ARCH_RNG_H */
