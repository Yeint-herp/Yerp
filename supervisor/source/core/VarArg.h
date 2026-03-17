#ifndef SUPERVISOR_CORE_VARARG_H
#define SUPERVISOR_CORE_VARARG_H

typedef __builtin_va_list Core_VarArgs;

#define Core_VarArgStart(args)        __builtin_va_start(args, 0)
#define Core_VarArgEnd(args)          __builtin_va_end(args)
#define Core_VarArgCopy(dest, source) __builtin_va_copy(dest, source)

#define Core_VarArg(args, type) __builtin_va_arg(args, type)

#endif /* SUPERVISOR_CORE_VARARG_H */
