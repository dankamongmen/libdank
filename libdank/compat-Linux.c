#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <libdank/ersatz/compat.h>
#include <libdank/objects/logctx.h>

int Signalfd(int fd,const sigset_t *ss,int flags){
	int ret;

	if((ret = signalfd(fd,ss,flags)) < 0){
		moan("Couldn't generate signalfd from %d/%d\n",fd,flags);
	}
	return ret;
}

int Timerfd_create(int clockid,int flags){
	int ret;

	if((ret = timerfd_create(clockid,flags)) < 0){
		moan("Couldn't generate timerfd from %d/%d\n",clockid,flags);
	}
	return ret;
}

int Timerfd_settime(int fd,int flags,const struct itimerspec *newv,struct itimerspec *curv){
	int ret;

	if( (ret = timerfd_settime(fd,flags,newv,curv)) ){
		moan("Couldn't configure timerfd %d\n",fd);
	}
	return ret;
}
