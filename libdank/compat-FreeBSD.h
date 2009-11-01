#ifndef LIBDANK_LIB_COMPAT
#define LIBDANK_LIB_COMPAT

#ifdef __cplusplus
extern "C" {
#endif

#define LIB_COMPAT "FreeBSD"
#define LIB_COMPAT_FREEBSD

#define PURE __pure

#define MAP_ANONYMOUS MAP_ANON

#define MEMSTAT_METHOD_SYSCTL
#define CPUENUM_METHOD_SYSCTL
#define HAS_SYSCTLBYNAME

#define ETH_ALEN	6

#define PRINTF_SSIZET "%d"
#define PRINTF_TIDT "%lu"	// cast the pthread_t to (unsigned long)

#include <libdank/objects/logctx.h>

static inline int
backtrace(void **buffer __attribute__ ((unused)),
		int size __attribute__ ((unused))){
	bitch("functionality unavailable on FreeBSD\n");
	return -1;
}

static inline char **
backtrace_symbols(void * const *buffer __attribute__ ((unused)),
		int size __attribute__ ((unused))){
	bitch("functionality unavailable on FreeBSD\n");
	return NULL;
}

#include <sys/types.h>
#include <sys/time.h>

typedef struct dirent *scandir_arg;

int sendfile_compat(int,int,off_t *,size_t);

// we can't portably mremap(2) -- it's only available on Linux. on freebsd,
// however, we can mmap at a location hint; we need the prot, flags and fd
// info, though.
void *mremap_compat(int fd,void *,size_t,size_t,int,int);
void *mremap_fixed_compat(int fd,void *,size_t,size_t,void *,int,int);

#include <sys/event.h>

// A pthread_testcancel() has been added at the entry, since epoll() is a
// cancellation point on Linux. Perhaps better to disable cancellation across
// both?
static inline int
Kevent(int kq,struct kevent *changelist,int nchanges,struct kevent *eventlist,
		int nevents,const struct timespec *timeo){
	int ret;

	pthread_testcancel();
	while((ret = kevent(kq,changelist,nchanges,eventlist,nevents,timeo)) < 0){
		if(errno != EINTR){ // loop on EINTR
			moan("Error during kevent\n");
			break;
		}
	}
	return ret;
}

static inline int
Kqueue(void){
	int ret;

	if((ret = kqueue()) < 0){
		moan("Couldn't create kqueue fd\n");
	}
	return ret;
}

unsigned long pthread_self_getnumeric(void);

#ifdef __cplusplus
}
#endif

#endif
