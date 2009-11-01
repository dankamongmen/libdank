#ifndef LIBDANK_ARCH_TIMERS
#define LIBDANK_ARCH_TIMERS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Read the 64-bit time-stamp counter available on fifth-generation+ x86
uint_fast64_t x86_read_tsc(void);

#ifdef __cplusplus
}
#endif

#endif
