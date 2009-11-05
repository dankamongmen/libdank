#ifndef LIBDANK_MODULES_EVENTS_EVCORE
#define LIBDANK_MODULES_EVENTS_EVCORE

#include <pthread.h>
#include <libdank/ersatz/compat.h>

struct evthread;
struct evsource;
struct evectors;

// The system resources necessary for event notifications. Schemes in which one
// evhandler is used with one or more threads, using their own or shared
// evectorss, are emphasized (as opposed to threads using multiple evhandlers --
// this is not yet actively supported, due to presumed uselessness).
typedef struct evhandler {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int fd;
	struct evsource *fdarray,*sigarray;
	// These are ints to facilitate their regular comparison to other ints
	// (primarily file descriptors). They ought never be less than 0.
	int sigarraysize,fdarraysize;
	struct evthread *threadlist;
	struct evectors *externalvec;
} evhandler;

// Takes a flag parameter, a (possibly zero) union over the LIBDANK_FD_* enum
// (these flags map directly to those defined by Linux's epoll_create1()).
// create_evhandler() does not launch a thread, since the thread creating the
// evhandler might want to use it directly, together with calls to evmain().
evhandler *create_evhandler(int)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

// Convenience function to create an evhandler (passing its argument directly
// through to create_evhandler(), and immediately call spawn_evthread() on it.
evhandler *create_evthread(int)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

int add_evector_kevents(struct evectors *,const struct kevent *,unsigned)
	__attribute__ ((warn_unused_result)) __attribute__ ((nonnull));
int flush_evector_changes(evhandler *,struct evectors *)
	__attribute__ ((warn_unused_result)) __attribute__ ((nonnull (1)));

// Launch a thread dedicated to processing this evectors. Threads associated
// with an evhandler will be reaped early in its destructor.
int spawn_evthread(struct evhandler *)
	__attribute__ ((warn_unused_result)) __attribute__ ((nonnull (1)));

int destroy_evhandler(evhandler *);

typedef struct evthreadstats {
	uintmax_t evhandler_errors;
} evthreadstats;

#endif
