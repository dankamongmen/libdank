#ifndef LIBDANK_UTILS_FCNTL
#define LIBDANK_UTILS_FCNTL

#ifdef __cplusplus
extern "C" {
#endif

// F_GETFL, F_SETFL
int Fcntl_getstatusflags(int);
int Fcntl_setstatusflags(int,long);

static inline int
Fcntl_addstatusflags(int fd,long flags){
	int setflags;

	if((setflags = Fcntl_getstatusflags(fd)) < 0){
		return -1;
	}
	setflags |= flags;
	return Fcntl_setstatusflags(fd,setflags);
}

// F_GETFD, F_SETFD
int Fcntl_getfdflags(int);
int Fcntl_setfdflags(int,long);

static inline int
Fcntl_addfdflags(int fd,long flags){
	int setflags;

	if((setflags = Fcntl_getfdflags(fd)) < 0){
		return -1;
	}
	setflags |= flags;
	return Fcntl_setfdflags(fd,setflags);
}

// F_DUPFD, F_DUPFD_CLOEXEC
int Fcntl_dupfd(int,long);
int Fcntl_cloexec(int,long); // emulated outside Linux 2.6.24+

#ifdef __cplusplus
}
#endif

#endif
