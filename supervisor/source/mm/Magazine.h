#ifndef SUPERVISOR_MM_MAGAZINE_H
#define SUPERVISOR_MM_MAGAZINE_H

#define MM_PFA_MAGAZINE_SIZE 64

struct Mm_PfaMagazine
{
    u16 Count;
    u16 Reserved[3];

    u64 Pages[MM_PFA_MAGAZINE_SIZE];
};

#endif /* SUPERVISOR_MM_MAGAZINE_H */
