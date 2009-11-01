#ifndef APPS_STOP_STDSTOP
#define APPS_STOP_STDSTOP

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/apps/init.h>

void app_stop(const app_def *,app_ctx *,int) __attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif
