#ifndef MODULES_OOPS_OOPS
#define MODULES_OOPS_OOPS

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <sys/ucontext.h>
#include <libdank/objects/logctx.h>

// This attempts to be a global repository for information we'd want at crash
// time. We want this to be as simple as possible, to minimize the chances of
// faults causing problems once we're dumping this.

// Log all known fault information. Will attempt to detect which of known
// sigstacks its on, so call from fatal signal handler.
void log_oops(int,const siginfo_t *,const ucontext_t *);

// Change internal state reporting; starts at initializing...
void application_running(void);
void application_closing(void);

typedef void (*oops_stringizer)(void);

void add_oops_stringizer(oops_stringizer);

extern const char *last_main_task;

#define track_main(que)			\
	do{				\
		last_main_task = que;	\
	}while(0);

#ifdef __cplusplus
}
#endif

#endif
