#ifndef UTILS_FDS
#define UTILS_FDS

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <libdank/utils/fcntl.h>

int dup_dev_null(int);
int cork_fd(int);
int uncork_fd(int);
off_t fd_offset(int);

int Readn(int,void *,size_t);
int Writen(int,const void *,size_t);

// Linux has extended fd-generation functions (open(), accept(), epoll() etc)
// to take *_NONBLOCK/*_CLOEXEC flags. We use these on Linux, and emulate them
// elsewhere (including old Linux).
#define LIBDANK_FD_CLOEXEC	0x0001	// Set the close-on-exec flag
#define LIBDANK_FD_NONBLOCK	0x0002	// Set the non-blocking flag

static inline int
set_fd_nonblocking(int fd){
	return Fcntl_addstatusflags(fd,O_NONBLOCK);
}

static inline int
set_fd_close_on_exec(int fd){
	return Fcntl_addfdflags(fd,FD_CLOEXEC);
}

// Predicates on file descriptors
int fd_writeablep(int);
int fd_readablep(int);

static inline int
fd_cloexecp(int fd){
	int flags;

	if((flags = Fcntl_getfdflags(fd)) < 0){
		return -1;
	}
	return !!(flags & FD_CLOEXEC);
}

static inline int
fd_nonblockp(int fd){
	int flags;

	if((flags = Fcntl_getstatusflags(fd)) < 0){
		return -1;
	}
	return !!(flags & O_NONBLOCK);
}

#ifdef __cplusplus
}
#endif

#endif
