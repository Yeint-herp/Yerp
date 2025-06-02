#ifndef FORMAT_HH
#define FORMAT_HH

#include <arch/serial.hh>
#include <ktl/type_traits>
#include <ktl/string_view>
#include <ktl/slice>
#include <ktl/pair>
#include <ktl/atomic>

template<typename Serial = SerialCOM1>
class FmtBase {
public:
    static void put(char c) { Serial::put(c); }

    static void puts(const char* s) {
        while (*s) {
            if (*s == '\n') put('\r');
            put(*s++);
        }
    }

    static void put_hex(u64 v, int digits = sizeof(u64)*2) {
        static constexpr char hex[] = "0123456789ABCDEF";
        put('0'); put('x');
        for (int i = (digits - 1) * 4; i >= 0; i -= 4)
            put(hex[(v >> i) & 0xF]);
    }

    static void put_bin(u64 v, int bits = sizeof(u64)*8) {
        put('0'); put('b');
        for (int i = bits - 1; i >= 0; --i)
            put((v >> i) & 1 ? '1' : '0');
    }

    static void put_dec(u64 v) {
        char buf[24];
        int len = 0;
        do {
            buf[len++] = char('0' + (v % 10));
            v /= 10;
        } while (v && len < 24);
        while (len--) put(buf[len]);
    }

    static void print(char c)            { put(c); }
    static void print(const char* s)     { puts(s); }
    static void print(char* s)           { puts(s); }
    static void print(const wchar_t* ws) {
        while (*ws) put(static_cast<char>(*ws++));
    }
    static void print(wchar_t* ws) {
        while (*ws) put(static_cast<char>(*ws++));
    }

    template<typename T, usize N>
    static void print(const T (&arr)[N]) {
        put('[');
        for (usize i = 0; i < N; ++i) {
            print(arr[i]);
            if (i + 1 < N) puts(", ");
        }
        put(']');
    }

    template<typename T, usize Extent>
    static void print(const ktl::slice<T, Extent>& s) {
        put('[');
        for (typename ktl::slice<T, Extent>::size_type i = 0; i < s.size(); ++i) {
            print(s[i]);
            if (i + 1 < s.size())
                puts(", ");
        }
        put(']');
    }

    template<typename T1, typename T2>
    static void print(const ktl::pair<T1, T2>& p) {
        put('(');
        print(p.first);
        puts(", ");
        print(p.second);
        put(')');
    }

    template<typename T>
    static void print(const ktl::atomic<T>& a) {
        print(a.load());
    }


    template<usize N>
    static void print(const char (&s)[N]) {
        puts(s);
    }
    template<usize N>
    static void print(const wchar_t (&ws)[N]) {
        for (usize i = 0; i < N && ws[i]; ++i)
            put(static_cast<char>(ws[i]));
    }

    static void print(bool b)               { puts(b ? "true" : "false"); }
    static void print(u8 v)                 { put_dec(v); }
    static void print(u16 v)                { put_dec(v); }
    static void print(u32 v)                { put_dec(v); }
    static void print(u64 v)                { put_dec(v); }
    static void print(i8 v)                 { put_dec(static_cast<u64>(v)); }
    static void print(i16 v)                { put_dec(static_cast<u64>(v)); }
    static void print(i32 v)                { put_dec(static_cast<u64>(v)); }
    static void print(i64 v)                { put_dec(static_cast<u64>(v)); }

    static void print(const ktl::string_view& sv) {
        for (char c : sv)
            put(c);
    }

    static void printf(const char* fmt) {
        puts(fmt);
    }

    template<typename T, typename... Ts>
    static void printf(const char* fmt,
                       const T& value,
                       const Ts&... rest)
    {
        const char* p = fmt;
        while (*p) {
            if (p[0] == '{' && p[1] == '{') {
                put('{'); p += 2;
            } else if (p[0] == '{') {
                ++p;
                const char* spec_start = p;
                while (*p && *p != '}') ++p;
                int spec_len = p - spec_start;
                if (*p == '}') ++p;
                print_with_spec(value, spec_start, spec_len);
                printf(p, rest...);
                return;
            } else {
                if (*p == '\n') put('\r');
                put(*p++);
            }
        }
    }

private:
    struct FormatSpec {
        bool alternate = false;
        bool zero_pad  = false;
        int  width     = 0;
        char type      = 0;
    };

    static FormatSpec parse_spec(const char* spec, int len) {
        FormatSpec fs;
        int i = 0;
        if (i < len && spec[i] == ':') ++i;
        if (i < len && spec[i] == '#') { fs.alternate = true;  ++i; }
        if (i < len && spec[i] == '0') { fs.zero_pad  = true;  ++i; }
        while (i < len && spec[i] >= '0' && spec[i] <= '9') {
            fs.width = fs.width * 10 + (spec[i] - '0');
            ++i;
        }
        if (i < len) fs.type = spec[i];
        return fs;
    }

    template<typename T>
    static void print_with_spec(const T& v,
                                const char* spec,
                                int spec_len)
    {
        auto fs = parse_spec(spec, spec_len);
        route(v, fs.alternate, fs.zero_pad, fs.width, fs.type);
    }

    template<typename T>
    static void route(const T& v,
                      bool alternate,
                      bool zero_pad,
                      int width,
                      char type)
    {
        using U = typename ktl::remove_reference<T>::type;

        if constexpr (ktl::is_array<U>::value) {
            print(v);

        } else if constexpr (ktl::is_integral_v<U>) {
            u64 val = static_cast<u64>(v);
            if (type == 'x' || type == 'X') {
                if (alternate) puts(type == 'x' ? "0x" : "0X");
                int len; auto buf = make_hex_string(val, type=='x', len);
                print_padded(buf, len, width, zero_pad);
            } else if (type == 'b') {
                if (alternate) puts("0b");
                int len; auto buf = make_bin_string(val, len);
                print_padded(buf, len, width, zero_pad);
            } else {
                int len; auto buf = make_dec_string(val, len);
                print_padded(buf, len, width, zero_pad);
            }

        } else if constexpr (ktl::is_same_v<U, const char*> ||
                             ktl::is_same_v<U, char*>) {
            print(v);

        } else if constexpr (ktl::is_same_v<U, const wchar_t*> ||
                             ktl::is_same_v<U, wchar_t*>) {
            print(v);

        } else if constexpr (ktl::is_same_v<U, ktl::string_view>) {
            print(v);

        } else if constexpr (ktl::is_pointer_v<U>) {
            u64 val = static_cast<u64>(reinterpret_cast<uptr>(v));
            int len; auto buf = make_hex_string(val, false, len);
            print_padded(buf, len, width, zero_pad);
        
        } else if constexpr (ktl::is_instantiation_of_v<ktl::atomic, U>) {
            print(v);

        } else if constexpr (ktl::is_slice_v<U>) {
            print(v);

        } else if constexpr (ktl::is_pair_v<U>) {
            print(v);
        } else {
            puts("[unsupported]");
        }
    }

    static const char* make_hex_string(u64 v,
                                       bool lowercase,
                                       int& out_len)
    {
        static char buf[16];
        static constexpr char upper[] = "0123456789ABCDEF";
        static constexpr char lower[] = "0123456789abcdef";
        auto const* table = lowercase ? lower : upper;

        if (v == 0) {
            buf[0] = '0';
            out_len = 1;
            return buf;
        }

        int idx = 0;
        while (v && idx < 16) {
            buf[idx++] = table[v & 0xF];
            v >>= 4;
        }
        for (int i = 0; i < idx/2; ++i)
            ktl::swap(buf[i], buf[idx-1-i]);

        out_len = idx;
        return buf;
    }

    static const char* make_bin_string(u64 v,
                                       int& out_len)
    {
        static char buf[64];
        if (v == 0) {
            buf[0] = '0';
            out_len = 1;
            return buf;
        }

        int idx = 0;
        while (v && idx < 64) {
            buf[idx++] = (v & 1) ? '1' : '0';
            v >>= 1;
        }
        for (int i = 0; i < idx/2; ++i)
            ktl::swap(buf[i], buf[idx-1-i]);

        out_len = idx;
        return buf;
    }

    static const char* make_dec_string(u64 v,
                                       int& out_len)
    {
        static char buf[24];
        int len = 0;
        do {
            buf[len++] = char('0' + (v % 10));
            v /= 10;
        } while (v && len < 24);
        for (int j = 0; j < len/2; ++j)
            ktl::swap(buf[j], buf[len-1-j]);
        out_len = len;
        return buf;
    }

    static void print_padded(const char* str,
                             int len,
                             int width,
                             bool zero_pad)
    {
        int pad = width - len;
        if (pad > 0) {
            char c = zero_pad ? '0' : ' ';
            while (pad--) put(c);
        }
        for (int i = 0; i < len; ++i)
            put(str[i]);
    }
};

using Fmt = FmtBase<>;

#endif // FORMAT_HH
