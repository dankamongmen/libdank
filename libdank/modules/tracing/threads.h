#ifndef MODULES_TRACING_THREADS
#define MODULES_TRACING_THREADS

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/utils/threads.h>

typedef void (*logged_tfxn)(void *);

// thread spawning wrappers which register with the debugging and log systems
int new_traceable_thread(const char *,pthread_t *,logged_tfxn,void *);
int new_traceable_detached(const char *,logged_tfxn,void *);

// cancel and join on the thread.  a join will be attempted whether the cancel
// is successful or not.  bludgeon() will be called if non-NULL.
int reap_traceable_thread(const char *,pthread_t,cancel_helper);

// the same, without cancellation or the function call
int join_traceable_thread(const char *,pthread_t);

void log_tstk(void *stack);

#ifdef __cplusplus
}
#endif

#endif
