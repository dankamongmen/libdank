#ifndef ARCH_CPU
#define ARCH_CPU

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int id_cpu(void);

// returns 0 on failure (can't determine cacheline size)
size_t align_size(size_t);

#ifdef __cplusplus
}
#endif

#endif
