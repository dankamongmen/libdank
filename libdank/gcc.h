#ifndef LIBDANK_GCC
#define LIBDANK_GCC

#include <features.h>

#ifdef __GNUC_PREREQ
# if __GNUC_PREREQ (3, 3)
#  define NOTHROW __attribute__ ((__nothrow__))
# else
#  define NOTHROW
# endif
# if __GNUC_PREREQ (2, 96)
#  define PUREFUNC __attribute__ ((pure))
# else
#  define PUREFUNC
# endif
# if __GNUC_PREREQ (2, 95)
#  define LIKELY(expr) __builtin_expect((expr),1)
#  define UNLIKELY(expr) __builtin_expect((expr),0)
# else
#  define LIKELY(expr) (expr)
#  define UNLIKELY(expr) (expr)
#  warning "__builtin_expect not supported on this GCC"
# endif
# if __GNUC_PREREQ (2, 5)
#  define CONSTFUNC __attribute__ ((const))
# else
#  define CONSTFUNC
# endif
#else
# define CONSTFUNC
# define PUREFUNC
# define NOTHROW
# define LIKELY(expr) (expr)
# define UNLIKELY(expr) (expr)
# warning "__builtin_expect not supported on this compiler"
#endif

#endif
