#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

int cork_fd(int fd){
#ifdef TCP_CORK
        int cork = 1;

        return Setsockopt(fd,IPPROTO_TCP,TCP_CORK,&cork,sizeof(cork));
#else
#ifdef TCP_NOPUSH
        int nopush = 1;

        return Setsockopt(fd,IPPROTO_TCP,TCP_NOPUSH,&nopush,sizeof(nopush));
#else
#error "No methodology known for corking file descriptors"
#endif
#endif
}

int uncork_fd(int fd){
#ifdef TCP_CORK
        int cork = 0;

        return Setsockopt(fd,IPPROTO_TCP,TCP_CORK,&cork,sizeof(cork));
#else
#ifdef TCP_NOPUSH
        int nopush = 0;

        return Setsockopt(fd,IPPROTO_TCP,TCP_NOPUSH,&nopush,sizeof(nopush));
#else
#error "No methodology known for uncorking file descriptors"
#endif
#endif
}

int fd_writeablep(int fd){
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 0, };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(fd,&fds);
	return Select(fd + 1,NULL,&fds,NULL,&timeout);
}

int fd_readablep(int fd){
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 0, };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(fd,&fds);
	return Select(fd + 1,&fds,NULL,NULL,&timeout);
}

off_t fd_offset(int fd){
	return Lseek(fd,0,SEEK_CUR);
}

int dup_dev_null(int fd){
	int nullfd,ret = 0;

	if(fd < 0){
		return -1;
	}
	if((nullfd = open("/dev/null",O_RDWR)) < 0){
		return -1;
	}
	ret |= dup2(nullfd,fd);
	ret |= close(nullfd);
	return ret;
}

// FIXME fundamentally fucked API. Need to return s on success, 0 on EOF.
// Right now, we can't tell the difference. -1 is fine for 0 < X < s, but s
// must be returned for X = s. Audit all callers and change.
int Readn(int fd,void *buf,size_t unsafe_s){
	ssize_t off = 0,ret,s;

	if(unsafe_s > INT_MAX){
		bitch("Assertion failure: s = %zu > %d\n",unsafe_s,INT_MAX);
		return -1;
	}
	s = unsafe_s;
	while(off < s){
		if((ret = read(fd,(char *)buf + off,unsafe_s - off)) < 0){
			if(errno == EINTR){
				continue;
			}
			moan("Error reading %zd/%zd from %d\n",s - off,s,fd);
			return -1;
		}else if(ret == 0){
			bitch("EOF at %zd/%zd bytes from %d\n",s - off,s,fd);
			return 0;
		}
		off += ret;
	}
	return 0;
}

// Writes s bytes from buf to descriptor sd using socket semantics (write may
// not complete in one call). Returns s, or -1 if EOF is detected prior to all
// data being written or on error. Not to be used with nonblocking fds!
int Writen(int fd,const void *buf,size_t unsafe_s){
	ssize_t off = 0,s;

	if(unsafe_s > INT_MAX){
		bitch("Assertion failure: s = %zu > %d\n",unsafe_s,INT_MAX);
		return -1;
	}
	s = unsafe_s;
	while(off < s){
		ssize_t ret;

		if((ret = write(fd,(const char *)buf + off,(size_t)(s - off))) < 0){
			if(errno == EINTR){
				continue;
			}
			moan("Error writing %zd/%zd to %d\n",s - off,s,fd);
			return -1;
		}else if(ret == 0){
			bitch("EOF at %zd/%zd bytes from %d\n",s - off,s,fd);
			return 0;
		}
		off += ret;
	}
	return 0;
}
