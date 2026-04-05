#ifndef SUPERVISOR_CORE_MEMORY_H
#define SUPERVISOR_CORE_MEMORY_H

void *Core_CopyMemory(void *dst, const void *src, usize size);
void *Core_MoveMemory(void *dst, const void *src, usize size);
void *Core_FillMemory(void *dst, u8 value, usize size);

i32 Core_CompareMemory(const void *a, const void *b, usize size);

void *Core_ZeroMemory(void *dst, usize size);

usize       Core_StringLength(const char *str);
i32         Core_CompareString(const char *a, const char *b);
i32         Core_CompareStringN(const char *a, const char *b, usize maxLen);
char       *Core_CopyString(char *dst, const char *src);
char       *Core_CopyStringN(char *dst, const char *src, usize maxLen);
const char *Core_FindChar(const char *str, char c);
const char *Core_FindCharReverse(const char *str, char c);
const char *Core_FindSubstring(const char *haystack, const char *needle);

char *Core_DuplicateString(const char *str, u32 tag);

char *Core_UnsignedToString(char *dst, u64 value, u32 base);
char *Core_IntegerToString(char *dst, i64 value, u32 base);

#endif /* SUPERVISOR_CORE_MEMORY_H */
