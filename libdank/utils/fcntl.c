#include <fcntl.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/fcntl.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

int Fcntl_getstatusflags(int fd){
	int ret;

	if((ret = fcntl(fd,F_GETFL)) < 0){
		moan("Coudln't get file status flags on %d\n",fd);
	}
	return ret;
}

int Fcntl_setstatusflags(int fd,long flags){
	int ret;

	if( (ret = fcntl(fd,F_SETFL,flags)) ){
		moan("Coudln't set file status flags to %lx on %d\n",flags,fd);
	}
	return ret;
}

int Fcntl_getfdflags(int fd){
	int ret;

	if((ret = fcntl(fd,F_GETFD)) < 0){
		moan("Coudln't get file descriptor flags on %d\n",fd);
	}
	return ret;
}

int Fcntl_setfdflags(int fd,long flags){
	int ret;

	if( (ret = fcntl(fd,F_SETFD,flags)) ){
		moan("Coudln't set file descriptor flags to %lx on %d\n",flags,fd);
	}
	return ret;
}

int Fcntl_dupfd(int fd,long newfdmin){
	int ret;

	if((ret = fcntl(fd,F_DUPFD,newfdmin)) < 0){
		moan("Coudln't duplicate %d to %ld+\n",fd,newfdmin);
	}
	return ret;
}

// emulated outside Linux 2.6.24+
int Fcntl_cloexec(int fd,long newfdmin){
	int ret;

#ifdef F_DUPFD_CLOEXEC
	if((ret = fcntl(fd,F_DUPFD_CLOEXEC,newfdmin)) < 0){
		moan("Coudln't duplicate %d to %ld+\n",fd,newfdmin);
	}
#else
	if((ret = Fcntl_dupfd(fd,newfdmin)) < 0){
		return ret;
	}
	if(set_fd_close_on_exec(ret)){
		Close(ret);
		return -1;
	}
#endif
	return ret;
}
