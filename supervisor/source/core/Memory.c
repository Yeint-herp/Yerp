#include <core/Memory.h>
#include <mm/Pool.h>

void *Core_CopyMemory(void *dst, const void *src, usize size)
{
    u8       *d = dst;
    const u8 *s = src;

    if (((usize)d | (usize)s) % sizeof size == 0)
    {
        while (size >= sizeof size)
        {
            *(usize *)d = *(const usize *)s;
            d += sizeof size;
            s += sizeof size;
            size -= sizeof size;
        }
    }

    while (size--)
        *d++ = *s++;

    return dst;
}

void *Core_MoveMemory(void *dst, const void *src, usize size)
{
    u8       *d = dst;
    const u8 *s = src;

    if (d == s || size == 0)
        return dst;

    if (d < s || d >= s + size)
        return Core_CopyMemory(dst, src, size);

    d += size;
    s += size;

    if (((usize)d | (usize)s) % sizeof size == 0)
    {
        while (size >= sizeof size)
        {
            d -= sizeof size;
            s -= sizeof size;
            size -= sizeof size;
            *(usize *)d = *(const usize *)s;
        }
    }

    while (size--)
        *--d = *--s;

    return dst;
}

void *Core_FillMemory(void *dst, u8 value, usize size)
{
    u8 *d = dst;

    if (size >= sizeof size)
    {
        usize word = value;
        for (usize i = 8; i < sizeof size * 8; i <<= 1)
            word |= word << i;

        while (((uptr)d % sizeof size) != 0 && size)
        {
            *d++ = value;
            size--;
        }

        while (size >= sizeof size)
        {
            *(usize *)d = word;
            d += sizeof size;
            size -= sizeof size;
        }
    }

    while (size--)
        *d++ = value;

    return dst;
}

i32 Core_CompareMemory(const void *a, const void *b, usize size)
{
    const u8 *pa = a;
    const u8 *pb = b;

    while (size--)
    {
        if (*pa != *pb)
            return *pa - *pb;

        pa++;
        pb++;
    }

    return 0;
}

void *Core_ZeroMemory(void *dst, usize size)
{
    return Core_FillMemory(dst, 0, size);
}

void *memcpy(void *dst, const void *src, usize size) __attribute__((alias("Core_CopyMemory")));
void *memmove(void *dst, const void *src, usize size) __attribute__((alias("Core_MoveMemory")));
void *memset(void *dst, int value, usize size) __attribute__((alias("Core_FillMemory")));
int   memcmp(const void *a, const void *b, usize size) __attribute__((alias("Core_CompareMemory")));

usize Core_StringLength(const char *str)
{
    const char *p = str;

    while (*p)
        p++;

    return p - str;
}

i32 Core_CompareString(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }

    return *a - *b;
}

i32 Core_CompareStringN(const char *a, const char *b, usize maxLen)
{
    if (maxLen == 0)
        return 0;

    while (maxLen-- > 1 && *a && *a == *b)
    {
        a++;
        b++;
    }

    return *a - *b;
}

char *Core_CopyString(char *dst, const char *src)
{
    char *d = dst;

    while ((*d++ = *src++))
        ;

    return dst;
}

char *Core_CopyStringN(char *dst, const char *src, usize maxLen)
{
    char *d = dst;

    if (maxLen == 0)
        return dst;

    while (maxLen-- > 1 && *src)
        *d++ = *src++;

    *d = '\0';

    return dst;
}

const char *Core_FindChar(const char *str, char c)
{
    while (*str)
    {
        if (*str == c)
            return str;

        str++;
    }

    return c == '\0' ? str : nullptr;
}

const char *Core_FindCharReverse(const char *str, char c)
{
    const char *last = nullptr;

    while (*str)
    {
        if (*str == c)
            last = str;

        str++;
    }

    return c == '\0' ? str : last;
}

const char *Core_FindSubstring(const char *haystack, const char *needle)
{
    if (*needle == '\0')
        return haystack;

    usize needleLen = Core_StringLength(needle);

    while (*haystack)
    {
        if (*haystack == *needle)
            if (Core_CompareStringN(haystack, needle, needleLen) == 0)
                return haystack;

        haystack++;
    }

    return nullptr;
}

char *Core_DuplicateString(const char *str, u32 tag)
{
    if (!str)
        return nullptr;

    usize len = Core_StringLength(str) + 1;
    char *dup = Ex_Allocate(len, tag);

    if (dup)
        Core_CopyMemory(dup, str, len);

    return dup;
}
