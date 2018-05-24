#include <libdank/utils/fds.h>
#include <libdank/utils/maxfds.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/events/evcore.h>
#include <libdank/modules/events/signals.h>
#include <libdank/modules/events/sources.h>
#include <libdank/modules/tracing/threads.h>

#define EVTHREAD_SIGNAL SIGURG

static const char *EVTHREAD_NAME = "evhandler";

// In our builtin threading model, each thread has their own evectors, and
// share a single evhandler. That way, there's no locking on the evectors.
typedef struct evthread {
	struct evectors *ev;
	evthreadstats stats;
	pthread_t tid;
	struct evthread *next;
} evthread;

// State necessary for changing the domain of events and/or having them
// reported. One is required to do any event handling, under any scheme, and
// many plausible schemes will employ multiple evectorss.
typedef struct evectors {
	// compat-<OS>.h provides a kqueue-like interface (in terms of change
	// vectorization, which linux doesn't support) for non-BSD's
#ifdef LIB_COMPAT_LINUX
	struct kevent eventv,changev;
#else
	struct kevent *eventv,*changev;
#endif
	unsigned vsizes,changesqueued;
} evectors;

#ifdef LIB_COMPAT_LINUX
#define PTR_TO_EVENTV(ev) (&(ev)->eventv)
#define PTR_TO_CHANGEV(ev) (&(ev)->changev)
typedef struct epoll_event kevententry;
#define KEVENTENTRY_FD(kptr) ((k)->data.fd)
#else
#define PTR_TO_EVENTV(ev) ((ev)->eventv)
#define PTR_TO_CHANGEV(ev) ((ev)->changev)
typedef struct kevent kevententry;
#define KEVENTENTRY_FD(kptr) ((int)(k)->ident)
#define KEVENTENTRY_SIG(kptr) ((int)(k)->ident) // Doesn't exist on Linux
#endif

static inline const kevententry *
nth_kevent(const evectors *ev,unsigned n){
#ifdef LIB_COMPAT_LINUX
	return &ev->eventv.events[n];
#else
	return &ev->eventv[n];	
#endif
}

// We don't really want one per possible fd, necessarily; grab a page or a few
// pages' worth. The number of fds doesn't actually correspond tightly to the
// maximum possible events (due to non-fd-based events on FreeBSD) nor maximum
// possible change operations (due to multiple events per fd). FIXME
#ifdef LIB_COMPAT_LINUX
static int
create_evector(struct kevent *kv){
	if((kv->events = allocate_per_possible_fd("eventvector",sizeof(*kv->events))) == NULL){
		return -1;
	}
	if((kv->ctldata = allocate_per_possible_fd("ctlvector",sizeof(*kv->ctldata))) == NULL){
		Free(kv->events);
		return -1;
	}
	return 0;
}
#endif

static void
#ifdef LIB_COMPAT_LINUX
destroy_evector(struct kevent *kv){
	Free(kv->events);
	Free(kv->ctldata);
#else
destroy_evector(struct kevent **kv){
	Free(*kv);
#endif
}

static evectors *
create_evectors(void){
	evectors *ret;
	int maxfds;

	if((maxfds = determine_max_fds()) <= 0){
		return NULL;
	}
	if((ret = Malloc("eventcore",sizeof(*ret))) == NULL){
		return NULL;
	}
#ifdef LIB_COMPAT_LINUX
	if(create_evector(&ret->eventv)){
		Free(ret);
		return NULL;
	}
	if(create_evector(&ret->changev)){
		destroy_evector(&ret->eventv);
		Free(ret);
		return NULL;
	}
#else
	ret->eventv = allocate_per_possible_fd("eventvector",sizeof(*ret->eventv));
	ret->changev = allocate_per_possible_fd("changevector",sizeof(*ret->changev));
	if(!ret->eventv || !ret->changev){
		Free(ret->changev);
		Free(ret->eventv);
		Free(ret);
		return NULL;
	}
#endif
	ret->changesqueued = 0;
	ret->vsizes = maxfds;
	return ret;
}

// We do not enforce, but do expect and require:
//  - EV_ADD/EPOLL_CTL_ADD to be used as the control operation
//  - EPOLLET/EV_CLEAR to be used in the flags
int add_evector_kevents(evectors *e,const struct kevent *k,unsigned kcount){
	if(e->changesqueued + kcount >= e->vsizes){
		// FIXME be more robust. try to flush the changes to the
		// evhandler, or reallocate larger vectors. at least add stat!
		bitch("Couldn't add event (already have %u)\n",e->changesqueued);
		return -1;
	}
#ifdef LIB_COMPAT_LINUX
	memcpy(&e->changev.ctldata[e->changesqueued],k->ctldata,
			sizeof(*k->ctldata) * kcount);
	memcpy(&e->changev.events[e->changesqueued],k->events,
			sizeof(*k->events) * kcount);
#else
	memcpy(&e->changev[e->changesqueued],k,sizeof(*k) * kcount);
#endif
	e->changesqueued += kcount;
	return 0;
}

static void
destroy_evectors(evectors *e){
	if(e){
		nag("Destroying evector, %u change%s outstanding\n",
			e->changesqueued,e->changesqueued == 1 ? "" : "s");
		destroy_evector(&e->changev);
		destroy_evector(&e->eventv);
		Free(e);
	}
}

int flush_evector_changes(evhandler *eh,evectors *ev){
	int ret;

#ifdef LIB_COMPAT_LINUX
	ret = Kevent(eh->fd,&ev->changev,ev->changesqueued,NULL,0,NULL);
#else
	ret = Kevent(eh->fd,ev->changev,ev->changesqueued,NULL,0,NULL);
#endif
	// FIXME...
	ev->changesqueued = 0;
	ret |= Pthread_mutex_unlock(&eh->lock);
	return ret;
}

static int
add_evhandler_baseevents(evhandler *e){
	evectors *ev;

	if((ev = create_evectors()) == NULL){
		return -1;
	}
	if(add_signal_to_evcore(e,ev,EVTHREAD_SIGNAL,NULL,NULL)){
		destroy_evectors(ev);
		return -1;
	}
#ifdef LIB_COMPAT_LINUX
	if(Kevent(e->fd,&ev->changev,ev->changesqueued,NULL,0,NULL)){
#else
	if(Kevent(e->fd,ev->changev,ev->changesqueued,NULL,0,NULL)){
#endif
		destroy_evectors(ev);
		return -1;
	}
	ev->changesqueued = 0;
	e->externalvec = ev;
	return 0;
}

static int
initialize_evhandler(evhandler *e,int fd){
	if(Pthread_mutex_init(&e->lock,NULL)){
		goto err;
	}
	if(Pthread_cond_init(&e->cond,NULL)){
		goto lockerr;
	}
	e->threadlist = NULL;
	e->fd = fd;
	e->fdarraysize = determine_max_fds();
	if((e->fdarray = create_evsources(e->fdarraysize)) == NULL){
		goto conderr;
	}
	// Need we really go all the way through SIGRTMAX? FreeBSD 6 doesn't
	// even define it argh! FIXME
#ifdef SIGRTMAX
	e->sigarraysize = SIGRTMAX;
#else
	e->sigarraysize = SIGUSR2;
#endif
	if((e->sigarray = create_evsources(e->sigarraysize)) == NULL){
		goto fderr;
	}
	if(add_evhandler_baseevents(e)){
		goto sigerr;
	}
	return 0;

sigerr:
	destroy_evsources(e->sigarray,e->sigarraysize);
fderr:
	destroy_evsources(e->fdarray,e->fdarraysize);
conderr:
	Pthread_cond_destroy(&e->cond);
lockerr:
	Pthread_mutex_destroy(&e->lock);
err:
	return -1;
}

evhandler *create_evhandler(int flags){
	evhandler *ret;
	int fd;

	if(flags){
		if(flags != (flags & (LIBDANK_FD_CLOEXEC | LIBDANK_FD_NONBLOCK))){
			bitch("Invalid flags: %x\n",flags);
			return NULL;
		}
	}
#ifdef LIB_COMPAT_LINUX
	{
		int trueflags = 0;

		if(flags & LIBDANK_FD_CLOEXEC){
			trueflags |= EPOLL_CLOEXEC;
		}
		if((fd = epoll_create1(trueflags)) < 0){
			moan("Couldn't create epoll fd with flags %x\n",trueflags);
			return NULL;
		}
		if(flags & LIBDANK_FD_NONBLOCK){
			if(set_fd_nonblocking(fd)){
				Close(fd);
				return NULL;
			}
		}
	}
#else
	if((fd = Kqueue()) < 0){
		return NULL;
	}
	if(flags & LIBDANK_FD_CLOEXEC){
		nag("Emulating EPOLL_CLOEXEC\n");
		if(set_fd_close_on_exec(fd)){
			Close(fd);
			return NULL;
		}
	}
	if(flags & LIBDANK_FD_NONBLOCK){
		if(set_fd_nonblocking(fd)){
			Close(fd);
			return NULL;
		}
	}
#endif
	if( (ret = Malloc("eventcore",sizeof(*ret))) ){
		if(initialize_evhandler(ret,fd) == 0){
			return ret;
		}
		Free(ret);
	}
	Close(fd);
	return NULL;
}

evhandler *create_evthread(int flags){
	evhandler *ret;

	if( (ret = create_evhandler(flags)) ){
		if(spawn_evthread(ret) == 0){
			return ret;
		}
		destroy_evhandler(ret);
	}
	return NULL;
}

static void
signal_evthread(pthread_t tid){
	Pthread_kill(tid,EVTHREAD_SIGNAL);
}

// Cancel and join a set of evthreads launched via spawn_evthread().
static int
destroy_evthreadlist(evhandler *evh){
	evthread *e;
	int ret = 0;

	while( (e = evh->threadlist) ){
		ret |= reap_traceable_thread(EVTHREAD_NAME,e->tid,signal_evthread);
		destroy_evectors(e->ev);
		evh->threadlist = e->next;
		Free(e);
	}
	return ret;
}

int destroy_evhandler(evhandler *e){
	int ret = 0;

	if(e){
		ret |= destroy_evthreadlist(e);
		ret |= Pthread_mutex_destroy(&e->lock);
		ret |= Pthread_cond_destroy(&e->cond);
		ret |= destroy_evsources(e->sigarray,e->sigarraysize);
		ret |= destroy_evsources(e->fdarray,e->fdarraysize);
		destroy_evectors(e->externalvec);
		ret |= Close(e->fd);
		Free(e);
	}
	return ret;
}

typedef struct evthread_marshal {
	evhandler *eh;
	evthread *evth;
} evthread_marshal;

static inline evthread_marshal *
create_evthread_marshal(evhandler *eh,evthread *evth){
	evthread_marshal *ret;

	if( (ret = Malloc("evthread_marshal",sizeof(*ret))) ){
		ret->eh = eh;
		ret->evth = evth;
	}
	return ret;
}

static inline void
register_evthread(evhandler *eh,evthread *evth){
	pthread_mutex_lock(&eh->lock);
	evth->next = eh->threadlist;
	eh->threadlist = evth;
	pthread_mutex_unlock(&eh->lock);
}

static inline int
handle_read_event(const kevententry *k,evhandler *eh){
	int fd = KEVENTENTRY_FD(k);

	if(fd >= eh->fdarraysize || fd < 0){
		bitch("Received invalid fd %d\n",fd);
		// FIXME increment stat
		return 0;
	}
	nag("EVENT ON %d\n",fd);
	if(handle_evsource_read(eh->fdarray,fd)){
		return -1;
	}
	return 0;
}

static inline int
handle_write_event(const kevententry *k){
	nag("Write event on %d\n",KEVENTENTRY_FD(k));
	// FIXME
	return 0;
}

#ifdef LIB_COMPAT_FREEBSD
static inline int
handle_evfilt_signal(const kevententry *k,evhandler *eh){
	int sig = KEVENTENTRY_SIG(k);

	// nag("Received signal %d (%s)\n",sig,strsignal(sig));
	if(sig >= eh->sigarraysize || sig < 0){
		bitch("Received invalid signal %d\n",sig);
		// FIXME increment stat
		return 0;
	}
	// FIXME can one represent multiple signals? if so, do we get count?
	if(handle_evsource_read(eh->sigarray,sig)){
		// FIXME increment stat
		// FIXME do what?
		return 0;
	}
	return 0;
}

static inline int
handle_evfilt_timer(const kevententry *k){
	nag("Timer %d expired\n",(int)k->ident);
	// FIXME
	return 0;
}
#endif

// events must be greater than 0. ev must have at least that many events.
static void
handle_events(int events,evhandler *eh,evectors *ev){
	while(events--){
		const kevententry *k = nth_kevent(ev,events);
		int ret = 0;

		// In Linux, everything is a file descriptor. Not so on
		// FreeBSD, where we first must determine the event filter in
		// use. On non-fd filters, multiplex through BSD structures to
		// the callbacks. FIXME use a lookup table for function here.
#ifdef LIB_COMPAT_FREEBSD
		if(k->filter == EVFILT_READ){
			ret = handle_read_event(k,eh);
		}else if(k->filter == EVFILT_WRITE){
			ret = handle_write_event(k);
		}else if(k->filter == EVFILT_SIGNAL){
			ret = handle_evfilt_signal(k,eh);
		}else if(k->filter == EVFILT_TIMER){
			ret = handle_evfilt_timer(k);
		}else{
			bitch("Unknown filter: %hd\n",k->filter);
		}
#else
		// Unlike FreeBSD, we can have multiple events per kevententry
		// using epoll. We want to stop processing on a non-zero
		// return, but otherwise handle each...
		if((k->events & EPOLLIN) && (ret = handle_read_event(k,eh)) ){
		}else if((k->events & EPOLLOUT) && (ret = handle_write_event(k)) ){
		}else if(ret){
			bitch("Unknown events: %ju\n",(uintmax_t)k->events);
		}
#endif
		// FIXME handle a non-zero ret (close the fd)
	}
}

static __attribute__ ((noreturn)) void
evmain(void *unsafe_emarshal){
	evthread_marshal *emarsh = unsafe_emarshal;
	evthread *evth = emarsh->evth;
	evhandler *eh = emarsh->eh;
	evectors *ev = evth->ev;

	Free(emarsh);
	emarsh = unsafe_emarshal = NULL;
	while(1){
		int events;

		events = Kevent(eh->fd,PTR_TO_CHANGEV(ev),ev->changesqueued,
				PTR_TO_EVENTV(ev),ev->vsizes,NULL);
		// FIXME when we get an error from Kevent, we need very precise
		// semantics...maybe changesqueued ought be a value-result...?
		// and how, precisely, ought we handle unrestartable errors?
		// associate the requesting session? might not be defined! argh
		ev->changesqueued = 0;
		if(events < 0){
			if(errno != EINTR){ // simply loop on EINTR
				// bump stat...FIXME
			}
		}else if(events){
			handle_events(events,eh,ev);
		}
	}
}

int spawn_evthread(evhandler *eh){
	evthread_marshal *emarsh;
	evthread *evth;

	if((evth = Malloc("evthread",sizeof(*evth))) == NULL){
		return -1;
	}
	if((evth->ev = create_evectors()) == NULL){
		Free(evth);
		return -1;
	}
	if((emarsh = create_evthread_marshal(eh,evth)) == NULL){
		destroy_evectors(evth->ev);
		Free(evth);
		return -1;
	}
	memset(&evth->stats,0,sizeof(evth->stats));
	if(new_traceable_thread(EVTHREAD_NAME,&evth->tid,evmain,emarsh)){
		Free(emarsh);
		destroy_evectors(evth->ev);
		Free(evth);
		return -1;
	}
	register_evthread(eh,evth);
	return 0;
}
