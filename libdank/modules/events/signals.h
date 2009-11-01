#ifndef LIBDANK_MODULES_EVENTS_SIGNALS
#define LIBDANK_MODULES_EVENTS_SIGNALS

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/modules/events/sources.h>

struct evectors;
struct evhandler;

int add_signal_to_evcore(struct evhandler *,struct evectors *,int,evcbfxn,void *)
	__attribute__ ((nonnull (1,2)));

int add_signal_to_evhandler(struct evhandler *,int,evcbfxn,void *)
	__attribute__ ((nonnull (1)));

#ifdef __cplusplus
}
#endif

#endif
