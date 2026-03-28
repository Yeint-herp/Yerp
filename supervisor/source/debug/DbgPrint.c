#include <arch/Io.h>
#include <core/Memory.h>
#include <core/Spinlock.h>
#include <core/VarArg.h>
#include <debug/DbgPrint.h>
#include <dsa/RingBuffer.h>

#define DBG_STACK_BUF_SIZE 512

void s_e9SinkerChar(char c)
{
    Arch_IoOut8(0xe9, (u8)c);
}

void s_e9SinkerString(const char *str, usize len)
{
    const u8 *it = (const u8 *)str;

    while ((usize)it - (usize)str < len)
        Arch_IoOut8(0xe9, *it++);
}

ringbuf_of(char, 4096 * 4096) s_RingBufSinkerStore = {};

void s_RingBufSinkerChar(char c)
{
    if (!Dsa_RingBufferPush(&s_RingBufSinkerStore, c))
        Dsa_RingBufferWrap(&s_RingBufSinkerStore);
}

void s_RingBufSinkerString(const char *str, usize len)
{
    const u8 *it = (const u8 *)str;

    while ((usize)it - (usize)str < len)
        s_RingBufSinkerChar(*it++);
}

Dbg_Sinker g_e9Sinker      = {s_e9SinkerChar, s_e9SinkerString};
Dbg_Sinker g_RingBufSinker = {s_RingBufSinkerChar, s_RingBufSinkerString};

Dbg_SinkerMask s_SinkerEnableMask     = 0;
Dbg_SinkerMask s_SinkerRegisteredMask = 0;

Dbg_Sinker    s_Sinkers[256] = {};
Core_Spinlock s_SinkLock     = {};

void Dbg_SetSinkerMask(Dbg_SinkerMask mask)
{
    s_SinkerEnableMask = mask;
}

Dbg_SinkerMask Dbg_GetSinkerMask(void)
{
    return s_SinkerEnableMask;
}

static Dbg_Level s_MinLevel = DBG_TRACE;

void Dbg_SetLevel(Dbg_Level level)
{
    s_MinLevel = level;
}

Dbg_Level Dbg_GetLevel(void)
{
    return s_MinLevel;
}

/// returns the bit where sinker was registered, all bits set implies failure.
Dbg_SinkerMask Dbg_RegisterSinker(Dbg_Sinker sinker)
{
    if (!~s_SinkerRegisteredMask)
        return (Dbg_SinkerMask)~0;

    usize index      = Bit_FirstClearIndex(s_SinkerRegisteredMask);
    s_Sinkers[index] = sinker;
    s_SinkerRegisteredMask |= (1u << index);
    s_SinkerEnableMask |= (1u << index);

    return (Dbg_SinkerMask)(1u << index);
}

void Dbg_PurgeSinkers(Dbg_SinkerMask mask)
{
    Dbg_SinkerMask bits = mask & s_SinkerRegisteredMask;
    while (bits)
    {
        usize index      = Bit_FirstSetIndex(bits);
        s_Sinkers[index] = (Dbg_Sinker){};
        s_SinkerRegisteredMask &= ~(1u << index);
        bits &= ~(1u << index);
    }
}

static void s_SinkStringLocked(Dbg_SinkerMask mask, const char *buf, usize len)
{
    if (len == 0)
        return;

    Dbg_SinkerMask active = mask & s_SinkerRegisteredMask;
    while (active)
    {
        usize index = Bit_FirstSetIndex(active);
        s_Sinkers[index].sinkString(buf, len);
        active &= ~(1u << index);
    }
}

void Dbg_FmtFlush(Dbg_FmtContext *ctx)
{
    if (ctx->Position == 0)
        return;

    if (ctx->sinkMask)
    {
        Arch_IrqFlags flags = Core_SpinlockAcquireIrq(&s_SinkLock);
        s_SinkStringLocked(ctx->sinkMask, ctx->Buffer, ctx->Position);
        Core_SpinlockReleaseIrq(&s_SinkLock, flags);
    }

    ctx->Position = 0;
}

void Dbg_FmtChar(Dbg_FmtContext *ctx, char c)
{
    if (ctx->Position >= ctx->Capacity)
    {
        if (ctx->sinkMask)
            Dbg_FmtFlush(ctx);
        else
            return;
    }

    ctx->Buffer[ctx->Position++] = c;
}

void Dbg_FmtString(Dbg_FmtContext *ctx, const char *str, usize len)
{
    const char *end = str + len;
    while (str < end)
    {
        usize space     = ctx->Capacity - ctx->Position;
        usize remaining = end - str;
        usize chunk     = remaining < space ? remaining : space;

        Core_CopyMemory(ctx->Buffer + ctx->Position, str, chunk);
        ctx->Position += chunk;
        str += chunk;

        if (ctx->Position >= ctx->Capacity)
        {
            if (ctx->sinkMask)
                Dbg_FmtFlush(ctx);
            else
                return;
        }
    }
}

static void s_FmtUnsigned(Dbg_FmtContext *ctx, u64 value, u32 base, bool uppercase, u32 minWidth, char pad,
                          bool leftAlign)
{
    char  tmp[64];
    usize pos = 0;

    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0)
        tmp[pos++] = '0';
    else
        while (value)
        {
            tmp[pos++] = digits[value % base];
            value /= base;
        }

    if (!leftAlign)
        for (usize i = pos; i < minWidth; i++)
            Dbg_FmtChar(ctx, pad);

    while (pos--)
        Dbg_FmtChar(ctx, tmp[pos]);

    if (leftAlign)
        for (usize i = pos + 1; i < minWidth; i++)
            Dbg_FmtChar(ctx, ' ');
}

static void s_FmtSigned(Dbg_FmtContext *ctx, i64 value, u32 base, bool uppercase, u32 minWidth, char pad,
                        bool leftAlign)
{
    if (value < 0)
    {
        Dbg_FmtChar(ctx, '-');
        if (minWidth > 0)
            minWidth--;

        s_FmtUnsigned(ctx, (u64)(-(value + 1)) + 1, base, uppercase, minWidth, pad, leftAlign);
    }
    else
        s_FmtUnsigned(ctx, (u64)value, base, uppercase, minWidth, pad, leftAlign);
}

typedef struct
{
    u32  width;
    u32  precision;
    char pad;
    bool leftAlign;
    bool altForm;
    bool hasPrecision;

    enum
    {
        LEN_DEFAULT,
        LEN_8,    // hh
        LEN_16,   // h
        LEN_32,   // l
        LEN_64,   // ll / q
        LEN_SIZE, // z
        LEN_PTR,  // p
    } length;
} FmtSpec;

static const char *s_ParseSpec(const char *fmt, FmtSpec *spec)
{
    spec->width        = 0;
    spec->precision    = 0;
    spec->pad          = ' ';
    spec->leftAlign    = false;
    spec->altForm      = false;
    spec->hasPrecision = false;
    spec->length       = LEN_DEFAULT;

    for (;;)
        if (*fmt == '-')
        {
            spec->leftAlign = true;
            fmt++;
        }
        else if (*fmt == '0')
        {
            spec->pad = '0';
            fmt++;
        }
        else if (*fmt == '#')
        {
            spec->altForm = true;
            fmt++;
        }
        else
            break;

    if (spec->leftAlign)
        spec->pad = ' ';

    while (*fmt >= '0' && *fmt <= '9')
    {
        spec->width = spec->width * 10 + (*fmt - '0');
        fmt++;
    }

    if (*fmt == '.')
    {
        spec->hasPrecision = true;
        fmt++;

        while (*fmt >= '0' && *fmt <= '9')
        {
            spec->precision = spec->precision * 10 + (*fmt - '0');
            fmt++;
        }
    }

    if (fmt[0] == 'h' && fmt[1] == 'h')
    {
        spec->length = LEN_8;
        fmt += 2;
    }
    else if (fmt[0] == 'h')
    {
        spec->length = LEN_16;
        fmt += 1;
    }
    else if (fmt[0] == 'l' && fmt[1] == 'l')
    {
        spec->length = LEN_64;
        fmt += 2;
    }
    else if (fmt[0] == 'l')
    {
        spec->length = LEN_32;
        fmt += 1;
    }
    else if (fmt[0] == 'q')
    {
        spec->length = LEN_64;
        fmt += 1;
    }
    else if (fmt[0] == 'z')
    {
        spec->length = LEN_SIZE;
        fmt += 1;
    }

    return fmt;
}

static u64 s_ReadUnsigned(FmtSpec *spec, Core_VarArgs ap)
{
    switch (spec->length)
    {
        case LEN_8:
            return (u8)Core_VarArg(ap, u32);
        case LEN_16:
            return (u16)Core_VarArg(ap, u32);
        case LEN_32:
            return Core_VarArg(ap, u32);
        case LEN_64:
            return Core_VarArg(ap, u64);
        case LEN_SIZE:
        case LEN_PTR:
            return Core_VarArg(ap, usize);
        default:
            return Core_VarArg(ap, u32);
    }
}

static i64 s_ReadSigned(FmtSpec *spec, Core_VarArgs ap)
{
    switch (spec->length)
    {
        case LEN_8:
            return (i8)Core_VarArg(ap, i32);
        case LEN_16:
            return (i16)Core_VarArg(ap, i32);
        case LEN_32:
            return Core_VarArg(ap, i32);
        case LEN_64:
            return Core_VarArg(ap, i64);
        case LEN_SIZE:
        case LEN_PTR:
            return Core_VarArg(ap, isize);
        default:
            return Core_VarArg(ap, i32);
    }
}

void Dbg_FmtVprintf(Dbg_FmtContext *ctx, const char *fmt, Core_VarArgs ap)
{
    while (*fmt)
    {
        if (*fmt != '%')
        {
            Dbg_FmtChar(ctx, *fmt++);
            continue;
        }

        fmt++;

        if (*fmt == '%')
        {
            Dbg_FmtChar(ctx, '%');
            fmt++;
            continue;
        }

        FmtSpec spec;
        fmt = s_ParseSpec(fmt, &spec);

        char conv = *fmt++;
        switch (conv)
        {
            case 'd':
            case 'i':
            {
                i64 val = s_ReadSigned(&spec, ap);
                s_FmtSigned(ctx, val, 10, false, spec.width, spec.pad, spec.leftAlign);
                break;
            }

            case 'u':
            {
                u64 val = s_ReadUnsigned(&spec, ap);
                s_FmtUnsigned(ctx, val, 10, false, spec.width, spec.pad, spec.leftAlign);
                break;
            }

            case 'x':
            case 'X':
            {
                u64  val   = s_ReadUnsigned(&spec, ap);
                bool upper = (conv == 'X');

                if (spec.altForm)
                {
                    Dbg_FmtChar(ctx, '0');
                    Dbg_FmtChar(ctx, upper ? 'X' : 'x');

                    if (spec.width > 2)
                        spec.width -= 2;
                    else
                        spec.width = 0;
                }

                s_FmtUnsigned(ctx, val, 16, upper, spec.width, spec.pad, spec.leftAlign);
                break;
            }

            case 'o':
            {
                u64 val = s_ReadUnsigned(&spec, ap);

                if (spec.altForm)
                {
                    Dbg_FmtChar(ctx, '0');

                    if (spec.width > 1)
                        spec.width--;
                    else
                        spec.width = 0;
                }

                s_FmtUnsigned(ctx, val, 8, false, spec.width, spec.pad, spec.leftAlign);
                break;
            }

            case 'b':
            {
                u64 val = s_ReadUnsigned(&spec, ap);

                if (spec.altForm)
                {
                    Dbg_FmtChar(ctx, '0');
                    Dbg_FmtChar(ctx, 'b');

                    if (spec.width > 2)
                        spec.width -= 2;
                    else
                        spec.width = 0;
                }

                s_FmtUnsigned(ctx, val, 2, false, spec.width, spec.pad, spec.leftAlign);
                break;
            }

            case 'p':
            {
                uptr val = Core_VarArg(ap, uptr);

                Dbg_FmtChar(ctx, '0');
                Dbg_FmtChar(ctx, 'x');

                s_FmtUnsigned(ctx, val, 16, false, sizeof val * 2, '0', false);
                break;
            }

            case 's':
            {
                if (spec.hasPrecision && spec.precision == 0)
                {
                    usize       len = Core_VarArg(ap, usize);
                    const char *str = Core_VarArg(ap, const char *);
                    if (!str)
                    {
                        str = "(null)";
                        len = 6;
                    }

                    if (!spec.leftAlign)
                        for (usize i = len; i < spec.width; i++)
                            Dbg_FmtChar(ctx, ' ');

                    Dbg_FmtString(ctx, str, len);

                    if (spec.leftAlign)
                        for (usize i = len; i < spec.width; i++)
                            Dbg_FmtChar(ctx, ' ');
                }
                else
                {
                    const char *str = Core_VarArg(ap, const char *);
                    if (!str)
                        str = "(null)";

                    usize len = 0;
                    if (spec.hasPrecision)
                        while (len < spec.precision && str[len])
                            len++;
                    else
                        while (str[len])
                            len++;

                    if (!spec.leftAlign)
                        for (usize i = len; i < spec.width; i++)
                            Dbg_FmtChar(ctx, ' ');

                    Dbg_FmtString(ctx, str, len);

                    if (spec.leftAlign)
                        for (usize i = len; i < spec.width; i++)
                            Dbg_FmtChar(ctx, ' ');
                }

                break;
            }

            case 'c':
            {
                char c = Core_VarArg(ap, i32);

                Dbg_FmtChar(ctx, c);
                break;
            }

            case 'B':
            {
                bool val = Core_VarArg(ap, i32);

                const char *str = val ? "true" : "false";
                usize       len = val ? 4 : 5;

                Dbg_FmtString(ctx, str, len);
                break;
            }

            case 'Z':
            {
                u64 val = s_ReadUnsigned(&spec, ap);

                static const char *suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB"};

                u32 unit  = 0;
                u64 whole = val;
                u32 frac  = 0;

                while (whole >= 1024 && unit < 4)
                {
                    frac = (whole % 1024) * 10 / 1024;
                    whole /= 1024;
                    unit++;
                }

                u32 digits = 1;
                u64 temp   = whole;
                while (temp >= 10)
                {
                    digits++;
                    temp /= 10;
                }

                u32 total_len = digits;
                if (unit > 0 && frac > 0)
                    total_len += 2;

                total_len += 1;
                total_len += (unit == 0) ? 1 : 3;

                if (!spec.leftAlign)
                    for (u32 i = total_len; i < spec.width; i++)
                        Dbg_FmtChar(ctx, ' ');

                s_FmtUnsigned(ctx, whole, 10, false, 0, ' ', false);

                if (unit > 0 && frac > 0)
                {
                    Dbg_FmtChar(ctx, '.');
                    Dbg_FmtChar(ctx, '0' + frac);
                }

                Dbg_FmtChar(ctx, ' ');

                const char *suf = suffixes[unit];
                while (*suf)
                    Dbg_FmtChar(ctx, *suf++);

                if (spec.leftAlign)
                    for (u32 i = total_len; i < spec.width; i++)
                        Dbg_FmtChar(ctx, ' ');

                break;
            }

            default:
            {
                Dbg_FmtChar(ctx, '%');
                Dbg_FmtChar(ctx, conv);
                break;
            }
        }
    }
}

void Dbg_Print(const char *fmt, ...)
{
    char           buf[DBG_STACK_BUF_SIZE];
    Dbg_FmtContext ctx = {
        .Buffer   = buf,
        .Capacity = sizeof buf,
        .Position = 0,
        .sinkMask = s_SinkerEnableMask,
    };

    Core_VarArgs ap;
    Core_VarArgStart(ap);
    Dbg_FmtVprintf(&ctx, fmt, ap);
    Core_VarArgEnd(ap);

    Dbg_FmtFlush(&ctx);
}

void Dbg_PrintEx(const char *fmt, Dbg_SinkerMask mask, ...)
{
    char           buf[DBG_STACK_BUF_SIZE];
    Dbg_FmtContext ctx = {
        .Buffer   = buf,
        .Capacity = sizeof buf,
        .Position = 0,
        .sinkMask = mask,
    };

    Core_VarArgs ap;
    Core_VarArgStart(ap);
    Dbg_FmtVprintf(&ctx, fmt, ap);
    Core_VarArgEnd(ap);

    Dbg_FmtFlush(&ctx);
}

usize Dbg_Snprintf(char *buf, usize size, const char *fmt, ...)
{
    Dbg_FmtContext ctx = {
        .Buffer   = buf,
        .Capacity = size > 0 ? size - 1 : 0,
        .Position = 0,
        .sinkMask = 0,
    };

    Core_VarArgs ap;
    Core_VarArgStart(ap);
    Dbg_FmtVprintf(&ctx, fmt, ap);
    Core_VarArgEnd(ap);

    if (size > 0)
        buf[ctx.Position] = '\0';

    return ctx.Position;
}

static const char *s_LevelTags[] = {
    [DBG_TRACE] = "TRACE", [DBG_DEBUG] = "DEBUG", [DBG_INFO] = "INFO ",
    [DBG_WARN] = "WARN ",  [DBG_ERROR] = "ERROR", [DBG_FATAL] = "FATAL",
};

void Dbg_VLogInner(Dbg_Level level, const char *module, const char *fmt, Core_VarArgs ap)
{
    char           buf[DBG_STACK_BUF_SIZE];
    Dbg_FmtContext ctx = {
        .Buffer   = buf,
        .Capacity = sizeof buf,
        .Position = 0,
        .sinkMask = s_SinkerEnableMask,
    };

    Dbg_FmtChar(&ctx, '[');
    const char *tag = s_LevelTags[level];
    while (*tag)
        Dbg_FmtChar(&ctx, *tag++);

    Dbg_FmtChar(&ctx, ']');
    Dbg_FmtChar(&ctx, ' ');

    while (*module)
        Dbg_FmtChar(&ctx, *module++);

    Dbg_FmtChar(&ctx, ':');
    Dbg_FmtChar(&ctx, ' ');

    Dbg_FmtVprintf(&ctx, fmt, ap);

    Dbg_FmtChar(&ctx, '\n');
    Dbg_FmtFlush(&ctx);
}

void Dbg_LogInner(Dbg_Level level, const char *module, const char *fmt, ...)
{
    Core_VarArgs ap;
    Core_VarArgStart(ap);
    Dbg_VLogInner(level, module, fmt, ap);
    Core_VarArgEnd(ap);
}
