#ifndef LIBDANK_MODULES_EVENTS_FDS
#define LIBDANK_MODULES_EVENTS_FDS

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/modules/events/sources.h>

struct evectors;
struct evhandler;

int add_fd_to_evcore(struct evhandler *,struct evectors *,int,
			evcbfxn,evcbfxn,void *)
	__attribute__ ((nonnull (1,2)));

int add_fd_to_evhandler(struct evhandler *,int,evcbfxn,evcbfxn,void *)
	__attribute__ ((nonnull (1)));

#ifdef __cplusplus
}
#endif

#endif
