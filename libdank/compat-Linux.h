#ifndef LIBDANK_LIB_COMPAT
#define LIBDANK_LIB_COMPAT

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/gcc.h>

#define LIB_COMPAT "Linux"
#define LIB_COMPAT_LINUX

#define PURE __attribute__ ((pure))

#ifndef __USE_GNU
extern char *strsignal (int __sig) NOTHROW;
#endif

#define MEMSTAT_METHOD_SYSINFO
#define CPUENUM_METHOD_PROC

typedef const struct dirent *scandir_arg;

#define PRINTF_SSIZET "%zd"
#define PRINTF_TIDT "%lu"

#include <execinfo.h>

#include <sys/sendfile.h>
#define sendfile_compat sendfile

#include <sys/mman.h>
static inline void *
mremap_compat(int freebsdfd __attribute__ ((unused)),void *a,size_t o,size_t n,
		int p __attribute__ ((unused)),int f __attribute__ ((unused))){
	return mremap(a,o,n,MREMAP_MAYMOVE);
}

static inline void *
mremap_fixed_compat(int freebsdfd __attribute__ ((unused)),void *a,size_t o,
			size_t n,void *newaddr,int p __attribute__ ((unused)),
			int f __attribute__ ((unused))){
	return mremap(a,o,n,MREMAP_MAYMOVE|MREMAP_FIXED,newaddr);
}

int Signalfd(int,const sigset_t *,int);

struct itimerspec;

int Timerfd_create(int,int);
int Timerfd_settime(int,int,const struct itimerspec *,struct itimerspec *);

#include <sys/epoll.h>

// To emulate FreeBSD's kevent interface, supply a marshalling of two vectors.
// One's the epoll_event vector we feed directly to epoll_wait(), the other the
// data necessary to iterate over epoll_ctl() upon entry. evchanges and events
// may (but needn't) alias.
//
// See kevent(7) on FreeBSD.
struct kevent { // each element an array, each array the same number of members
	struct epoll_ctl_data {
		int op;
	} *ctldata; // array of ctldata
	struct epoll_event *events; // array of epoll_events
};

#include <libdank/objects/logctx.h>

// Emulation of FreeBSD's kevent(2) notification mechanism
static inline int
Kevent(int epfd,struct kevent *changelist,int nchanges,struct kevent *eventlist,
		int nevents,const struct timespec *timeo){
	int ret = 0,timemsec,n;

	// nag("%d change%s, %d potential events on %d\n",nchanges,
	//		nchanges == 1 ? "" : "s",nevents,epfd);
	for(n = 0 ; n < nchanges ; ++n){
		if(epoll_ctl(epfd,changelist->ctldata[n].op,
				changelist->events[n].data.fd,
				&changelist->events[n]) < 0){
			// FIXME let's get things precisely defined...divine
			// and emulate a precise definition of bsd's kevent()
			moan("Error modifying event %d/%d\n",n + 1,nchanges);
			ret = -1;
		}
	}
	if(ret){
		return ret;
	}
	if(nevents == 0){
		return 0;
	}
	if(timeo){
		timemsec = timeo->tv_sec * 1000 + timeo->tv_nsec / 1000000;
	}else{
		timemsec = -1;
	}
	while((ret = epoll_wait(epfd,eventlist->events,nevents,timemsec)) < 0){
		if(errno != EINTR){ // loop on EINTR
			moan("Error waiting for events\n");
			break;
		}
	}
	return ret;
}

// Emulation of FreeBSD's kqueue(2) constructor
static inline int
Kqueue(void){
	const int flags = 0;
	int ret;

	if((ret = epoll_create(flags)) < 0){
		moan("Couldn't create epoll fd with flags %x\n",flags);
	}
	return ret;
}

// OpenSSL requires a numeric identifier for threads. On Linux (using
// LinuxThreads or NPTL), pthread_self() is sufficient.
#include <pthread.h>
static inline unsigned long
pthread_self_getnumeric(void){
	return pthread_self();
}

#ifdef __cplusplus
}
#endif

#endif
