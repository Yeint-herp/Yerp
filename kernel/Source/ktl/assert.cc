#include <ktl/assert>
#include <core/format.hh>

[[noreturn]] void ktl::assertion_failure(const char* expr,
                                           const char* file,
                                           int         line)
{
    Fmt::printf("Assertion failed: {}\n",       expr);
    Fmt::printf("  at {}:{}\n",                  file, line);

    __builtin_trap();
}
