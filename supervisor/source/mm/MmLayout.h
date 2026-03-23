#ifndef SUPERVISOR_MM_LAYOUT_H
#define SUPERVISOR_MM_LAYOUT_H

typedef struct Mm_VaLayout
{
    uptr HhdmBase;
    uptr HhdmSize;

    uptr PfnDbBase;
    uptr PfnDbSize;

    uptr KernelImageBase;
    uptr KernelImageSize;
} Mm_VaLayout;

const Mm_VaLayout *Mm_GetVaLayout(void);
void               Arch_MmLayoutInit();

#endif /* SUPERVISOR_MM_LAYOUT_H */
