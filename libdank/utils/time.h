#ifndef LIB_UTILS_TIME
#define LIB_UTILS_TIME

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/time.h>

void timeval_subtract(struct timeval *,const struct timeval *,const struct timeval *);
intmax_t timeval_subtract_usec(const struct timeval *,const struct timeval *);

#ifdef __cplusplus
}
#endif

#endif
