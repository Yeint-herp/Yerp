#ifndef SUPERVISOR_MM_LAYOUT_H
#define SUPERVISOR_MM_LAYOUT_H

typedef struct Mm_VaLayout
{
    uptr HhdmBase;
    uptr HhdmSize;

    uptr PfnDbBase;
    uptr PfnDbSize;

    uptr DynamicSpaceBase;
    uptr DynamicSpaceSize;

    uptr ModuleSpaceBase;
    uptr ModuleSpaceSize;

    uptr SupervisorImageBase;
    uptr SupervisorImageSize;
} Mm_VaLayout;

const Mm_VaLayout *Mm_GetVaLayout(void);

#endif /* SUPERVISOR_MM_LAYOUT_H */
