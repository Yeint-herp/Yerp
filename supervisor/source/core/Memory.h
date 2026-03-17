#ifndef SUPERVISOR_CORE_MEMORY_H
#define SUPERVISOR_CORE_MEMORY_H

void *Core_CopyMemory(void *dst, const void *src, usize size);
void *Core_MoveMemory(void *dst, const void *src, usize size);
void *Core_FillMemory(void *dst, u8 value, usize size);

i32 Core_CompareMemory(const void *a, const void *b, usize size);

void *Core_ZeroMemory(void *dst, usize size);

#endif /* SUPERVISOR_CORE_MEMORY_H */
