#ifndef SUPERVISOR_DEBUG_DBGPRINT_H
#define SUPERVISOR_DEBUG_DBGPRINT_H

#include <core/VarArg.h>

typedef u8 Dbg_SinkerMask;

typedef enum
{
    DBG_TRACE,
    DBG_DEBUG,
    DBG_INFO,
    DBG_WARN,
    DBG_ERROR,
    DBG_FATAL,
} Dbg_Level;

typedef struct
{
    void (*sinkChar)(char);
    void (*sinkString)(const char *, usize);
} Dbg_Sinker;

typedef struct
{
    char *Buffer;
    usize Capacity;
    usize Position;

    Dbg_SinkerMask sinkMask;
} Dbg_FmtContext;

extern Dbg_Sinker g_e9Sinker;
extern Dbg_Sinker g_RingBufSinker;

void           Dbg_SetSinkerMask(Dbg_SinkerMask mask);
Dbg_SinkerMask Dbg_GetSinkerMask(void);

void      Dbg_SetLevel(Dbg_Level level);
Dbg_Level Dbg_GetLevel(void);

/// returns the bit where sinker was registered, all bits set implies failure.
Dbg_SinkerMask Dbg_RegisterSinker(Dbg_Sinker sinker);

void Dbg_PurgeSinkers(Dbg_SinkerMask);

void Dbg_Print(const char *fmt, ...);
void Dbg_PrintEx(const char *fmt, Dbg_SinkerMask mask, ...);

void Dbg_LogInner(Dbg_Level level, const char *module, const char *fmt, ...);
void Dbg_VLogInner(Dbg_Level level, const char *module, const char *fmt, Core_VarArgs ap);

usize Dbg_BufferPrint(char *buf, usize size, const char *fmt, ...);

void Dbg_FmtChar(Dbg_FmtContext *ctx, char c);
void Dbg_FmtString(Dbg_FmtContext *ctx, const char *str, usize len);
void Dbg_FmtVprintf(Dbg_FmtContext *ctx, const char *fmt, Core_VarArgs ap);
void Dbg_FmtFlush(Dbg_FmtContext *ctx);

#define Dbg_Log(level, module, fmt, ...)                                                                               \
    ({                                                                                                                 \
        if ((DBG_##level) >= Dbg_GetLevel())                                                                           \
            Dbg_LogInner(DBG_##level, module, fmt " (%s:%d)", ##__VA_ARGS__, __FILE_NAME__, __LINE__);                 \
    })

#ifdef DBG_MODULE
#define Log(level, fmt, ...) Dbg_Log(level, DBG_MODULE, fmt, ##__VA_ARGS__)
#else
#define Log(level, fmt, ...) static_assert(0, "Log(level, fmt, ...) requires DBG_MODULE set!")
#endif

#endif /* SUPERVISOR_DEBUG_DBGPRINT_H */
