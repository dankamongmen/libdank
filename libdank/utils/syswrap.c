#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <locale.h>
#include <stdint.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

static void
socket_failure_helper(int pf_type,int sock_type,int protocol){
	const char *sockstr = "unknown";
	const char *pfstr = "unknown";

	if(pf_type == PF_UNIX){
		pfstr = "UNIX/LOCAL";
	}else if(pf_type == PF_INET6){
		pfstr = "INET6";
	}else if(pf_type == PF_INET){
		pfstr = "INET";
#ifdef PF_PACKET
	}else if(pf_type == PF_PACKET){
		pfstr = "PACKET";
#endif
	}

	if(sock_type == SOCK_STREAM){
		sockstr = "STREAM";
	}else if(sock_type == SOCK_DGRAM){
		sockstr = "DGRAM";
	}else if(sock_type == SOCK_SEQPACKET){
		sockstr = "SEQPACKET";
	}else if(sock_type == SOCK_RAW){
		sockstr = "RAW";
	}

	moan("Couldn't acquire a socket (family %s, type %s, prot %d)\n",
			pfstr,sockstr,protocol);
}

long Sysconf_named(const char *strname,int name){
	long ret;

	if((ret = sysconf(name)) < 0){
		if(errno == EINVAL){
			moan("Sysconf(%s==%d) failed\n",strname,name);
		}
		// Otherwise, negative values can be valid returns
	}
	return ret;
}

unsigned Alarm(unsigned seconds){
	unsigned ret;

	if( (ret = alarm(seconds)) ){
		nag("Preempted alarm set to expire in %us\n",ret);
	}
	return ret;
}

int Select(int fd,fd_set *r,fd_set *w,fd_set *e,struct timeval *tv){
	int ret;

	if((ret = select(fd,r,w,e,tv)) < 0){
		moan("Couldn't select on %d fds\n",fd);
	}
	return ret;
}

int Sigaction(int signum,const struct sigaction *sigact,struct sigaction *oldsa){
	if(sigaction(signum,sigact,oldsa)){
		moan("Couldn't set up signal handler for %s\n",strsignal(signum));
		return -1;
	}
	return 0;
}

int Fseek(FILE *stream,long offset,int whence){
	int ret;

	if( (ret = fseek(stream,offset,whence)) ){
		moan("Error fseeking %d-style to %ld\n",whence,offset);
	}
	return ret;
}

int Open(const char *fname,int flags){
	int fd;

	// use OpenCreat() to call open(2) with the O_CREAT flag, as it
	// accepts a mode_t argument (open(2) with O_CREAT needs a mode_t).
	// i do not wrap creat() itself because it is stupid. fuck posix.
	if(flags & O_CREAT){
		bitch("Must supply mode with creation flags\n");
		return -1;
	}
	if((fd = open(fname,flags)) < 0){
		moan("Couldn't open %s for 0x%x\n",fname,flags);
	}
	return fd;
}

int Shm_open(const char *key,int oflag,mode_t mode){
	int fd;

	if((fd = shm_open(key,oflag,mode)) < 0){
		moan("Couldn't open shared memory at %s for %d\n",key,oflag);
	}
	return fd;
}

int OpenCreat(const char *fname,int flags,mode_t mode){
	int fd;

	// use Open() to call open(2) without the O_CREAT flag, as it doesn't
	// accept the mode_t argument (open(2) with O_CREAT needs a mode_t).
	if((flags & O_CREAT) == 0){
		bitch("Mustn't supply mode without creation flag\n");
		return -1;
	}
	if((fd = open(fname,flags,mode)) < 0){
		moan("Couldn't open %s for 0x%x with perms %o\n",fname,flags,mode);
	}
	return fd;
}

off_t Lseek(int fd,off_t offset,int whence){
	off_t ret;

	if((ret = lseek(fd,offset,whence)) < 0){
		moan("Couldn't lseek using %s whence\n",whence == SEEK_SET ?
				"SEEK_SET" : whence == SEEK_CUR ? "SEEK_CUR" :
				whence == SEEK_END ? "SEEK_END" : "unknown");
	}
	return ret;
}

int Setpriority(int which,int who,int prio){
	int ret;

	if( (ret = setpriority(which,who,prio)) ){
		moan("Couldn't set priority %d/%d/%d\n",which,who,prio);
	}
	return ret;
}

unsigned Sleep(unsigned s){
	unsigned ret;

	if( (ret = sleep(s)) ){
		bitch("Woke up from sleep with %u seconds left\n",ret);
	}
	return ret;
}

#ifdef MEMSTAT_METHOD_SYSINFO
#include <sys/sysinfo.h>
int Sysinfo(struct sysinfo *info){
	if(sysinfo(info)){
		moan("Couldn't acquire system info at %p\n",info);
		return -1;
	}
	return 0;
}
#endif

void *Mmap(void *start,size_t length,int prot,int flags,int fd,off_t offset){
	void *ret;

	if((ret = mmap(start,length,prot,flags,fd,offset)) == MAP_FAILED){
		moan("Couldn't mmap %zu at %jd on %d\n",length,(intmax_t)offset,fd);
	}
	return ret;
}

ssize_t Readv(int fd,const struct iovec *iov,int iovlen){
	ssize_t ret;

	if((ret = readv(fd,iov,iovlen)) < 0){
		if(errno != EINTR){
			moan("Couldn't read from %d\n",fd);
		}
	}
	return ret;
}

ssize_t Writev(int fd,const struct iovec *iov,int iovlen){
	ssize_t ret;

	if((ret = writev(fd,iov,iovlen)) < 0){
		if(errno != EINTR){
			moan("Couldn't write to %d\n",fd);
		}
	}
	return ret;
}

// FIXME do something better with flags arg -- on Linux, if accept4() is not
// available, use them. on freebsd, either use them, or ignore them, or check
// whether they match what would occur if they were ignored....
int Accept4(int sd,struct sockaddr *addr,socklen_t *addrlen,int flags){
	int ret;

#ifdef SYS_accept4
	if((ret = accept4(sd,addr,addrlen,flags)) < 0){
#else
	if(flags){
		bitch("Can't set flags to old accept()\n");
	}
	if((ret = accept(sd,addr,addrlen)) < 0){
#endif
		if(errno != EAGAIN && errno != EWOULDBLOCK){
			moan("Error accepting on %d\n",sd);
		}
	}
	return ret;
}

int Daemon(const char *appname,int nochdir,int noclose){
	int ret;

	if( (ret = daemon(nochdir,noclose)) ){
		moan("Couldn't daemonize %s\n",appname);
	}
	return ret;
}

int Socketpair(int domain,int type,int protocol,int *sv){
	int ret;

	if( (ret = socketpair(domain,type,protocol,sv)) ){
		socket_failure_helper(domain,type,protocol);
	}
	return ret;
}

char *Setlocale(int category,const char *locale){
	char *ret;

	if((ret = setlocale(category,locale)) == NULL){
		if(strcmp(locale,"")){
			bitch("Couldn't set locale %s\n",locale);
		}else{
			bitch("Couldn't set default locale (are LC_* or LANG set?)\n");
		}
	}
	return ret;
}

pid_t Waitpid(pid_t pid,int *status,int options){
	pid_t ret;

	if((ret = waitpid(pid,status,options)) < 0){
		moan("Couldn't waitpid() using %d/%d\n",pid,options);
	}
	return ret;
}

ssize_t Read(int fd,void *buf,size_t s){
	ssize_t ret;

	if((ret = read(fd,buf,s)) < 0){
		moan("Error while reading %zu from %d\n",s,fd);
	}
	return ret;
}

int Recvmsg(int sd,struct msghdr *msg,int flags){
	int ret;

	if((ret = recvmsg(sd,msg,flags)) < 0){
		moan("Couldn't read message from %d with flags %d\n",sd,flags);
	}
	return ret;
}

ssize_t Write(int fd,const void *buf,size_t s){
	ssize_t ret;

	if((ret = write(fd,buf,s)) < 0){
		moan("Error while writing %zu to %d\n",s,fd);
	}
	return ret;
}

// Should be nfds_t, but that's not very widespread.
int Poll(struct pollfd *pfds,unsigned nfd,int timeout){
	int ret;

	if((ret = poll(pfds,nfd,timeout)) < 0){
		moan("Error while poll()ing %u fds\n",nfd);
	}
	return ret;
}

int Execv(const char *cmd,char * const argv[]){
	int ret;

	if(argv[0] == NULL){
		bitch("Didn't set argv[0]\n");
		return -1;
	}
	if( ((ret = execv(cmd,argv))) ){
		moan("Error launching %s\n",cmd);
	}
	return ret;
}

int Execvp(const char *cmd,char * const argv[]){
	int ret;

	if(argv[0] == NULL){
		bitch("Didn't set argv[0]\n");
		return -1;
	}
	if( ((ret = execvp(cmd,argv))) ){
		moan("Error launching %s\n",cmd);
	}
	return ret;
}

// socket() takes PF_*, unlike most everything else
int Socket(int pf_type,int sock_type,int protocol){
	int ret;

	if((ret = socket(pf_type,sock_type,protocol)) < 0){
		socket_failure_helper(pf_type,sock_type,protocol);
	}
	return ret;
}

#ifdef AF_PACKET
#include <linux/if_packet.h>
#endif

static void
handle_bind_err(int sd,const struct sockaddr *sa){
	switch(sa->sa_family){
		case AF_INET6: {
			char inet[INET6_ADDRSTRLEN];
			const struct sockaddr_in6 *sina;
			int err;

			err = errno;
			sina = (const struct sockaddr_in6 *)sa;
			inet_ntop(AF_INET6,&sina->sin6_addr.s6_addr,inet,sizeof(inet));
			errno = err;
			moan("Couldn't bind %d to %s:%hu\n",sd,
					inet,ntohs(sina->sin6_port));
		} break;
		case AF_INET: {
			char inet[INET_ADDRSTRLEN];
			const struct sockaddr_in *sina;
			uint32_t ip;
			int err;

			err = errno;
			sina = (const struct sockaddr_in *)sa;
			ip = sina->sin_addr.s_addr;
			inet_ntop(AF_INET,&ip,inet,sizeof(inet));
			errno = err;
			moan("Couldn't bind %d to %s:%hu\n",sd,
					inet,ntohs(sina->sin_port));
		} break;
		case AF_UNIX: {
			const struct sockaddr_un *sun;

			sun = (const struct sockaddr_un *)sa;
			moan("Couldn't bind %d to %s\n",sd,sun->sun_path);
		} break;
#ifdef AF_PACKET
		case AF_PACKET: {
			const struct sockaddr_ll *sll;

			sll = (const struct sockaddr_ll *)sa;
			moan("Couldn't bind %d to prots %d on iface %d\n",
					sd,sll->sll_protocol,sll->sll_ifindex);
		} break;
#endif
		default:
			moan("Couldn't bind %d to family %d\n",
					sd,sa->sa_family);
	}
}

int Bind(int sd,const struct sockaddr *sa,socklen_t slen){
	if(bind(sd,sa,slen)){
		handle_bind_err(sd,sa);
		return -1;
	}
	return 0;
}

int Getsockname(int sd,struct sockaddr *s,socklen_t *slen){
	int ret;

	if( (ret = getsockname(sd,s,slen)) ){
		moan("Couldn't get local socket name for %d\n",sd);
	}
	return ret;
}

int Sigaltstack(const stack_t *ss,stack_t *oss){
	if(sigaltstack(ss,oss)){
		if(ss){
			moan("Couldn't set %zub signal stack at %p.",
					ss->ss_size,ss->ss_sp);
		}
		if(oss){
			moan("Couldn't get signal stack at %p.",oss);
		}
		return -1;
	}
	return 0;
}

int Inet_pton(int af,const char *src,void *ip){
	int ret;

	if((ret = inet_pton(af,src,ip)) < 0){ // unsupported protocol
		moan("Couldn't convert address %s to numeric\n",src);
		return -1;
	}else if(ret == 0){ // malformed
		bitch("Couldn't convert invalid address %s\n",src);
		return -1;
	}
	return 0;
}

int Accept(int lsd,struct sockaddr *sa,socklen_t *slen){
	int sd,old_fam = 0;

	if(sa){
		if(*slen == 0){
			bitch("Passed 0-length socket len for %p\n",sa);
			return -1;
		}
		old_fam = sa->sa_family;
	}
	if((sd = accept(lsd,sa,slen)) < 0){
		if(errno != EAGAIN && errno != EWOULDBLOCK){
			moan("Couldn't accept on socket %d\n",lsd);
		}
		return -1;
	}
	if(old_fam && old_fam != sa->sa_family){
		bitch("Accept addrfamily difference: %d != %d\n",
				old_fam,sa->sa_family);
		Close(sd);
		return -1;
	}
	return sd;
}

// FreeBSD does not support a mremap() system call, so we must emulate it
// (hence the additional arguments relative to Linux's mremap()). The flags
// parameter refers to mmap() flags, *not* mremap() flags; we only support the
// case equivalent to Linux's MREMAP_MAYMOVE in this function (see
// Mremap_fixed() below for MREMAP_MAYMOVE|MREMAP_FIXED).
void *Mremap(int fd,void *oldaddr,size_t oldsize,size_t newsize,int prot,int flags){
	void *ret;

	if((ret = mremap_compat(fd,oldaddr,oldsize,newsize,prot,flags)) == MAP_FAILED){
		track_failloc();
		moan("Couldn't remap %zub to %zub from %p\n",oldsize,newsize,oldaddr);
	}
	return ret;
}

// Mremap() for the MREMAP_MAYMOVE|MREMAP_FIXED case.
void *Mremap_fixed(int fd,void *oldaddr,size_t oldsize,size_t newsize,
				void *newaddr,int prot,int flags){
	void *ret;

	ret = mremap_fixed_compat(fd,oldaddr,oldsize,newsize,newaddr,prot,flags);
	if(ret == MAP_FAILED){
		track_failloc();
		moan("Couldn't remap %zub to %zub from %p\n",oldsize,newsize,oldaddr);
	}
	return ret;
}

int Getsockopt(int sd,int level,int name,void *val,socklen_t *len){
	if(getsockopt(sd,level,name,val,len)){
		moan("Couldn't get %u-byte sockopt %d/%d on %d\n",
				*len,level,name,sd);
		return -1;
	}
	return 0;
}

int Sigprocmask(int how,const sigset_t *ns,sigset_t *os){
	int ret;

	if( (ret = sigprocmask(how,ns,os)) ){
		moan("Couldn't set proc sigmask (%d-style)\n",how);
	}
	return ret;
}

int Setsockopt(int sd,int lvl,int nm,const void *val,socklen_t len){
	if(setsockopt(sd,lvl,nm,val,len)){
		moan("Couldn't set %u-byte sockopt %d/%d on %d\n",
				len,lvl,nm,sd);
		return -1;
	}
	return 0;
}

int Connect(int sd,const struct sockaddr *sa,socklen_t slen){
	if(connect(sd,sa,slen)){
		moan("Couldn't connect on socket %d\n",sd);
		return -1;
	}
	return 0;
}

#ifdef HAS_SYSCTLBYNAME
#include <sys/sysctl.h>

int Sysctlbyname(const char *name,void *o,size_t *so,void *n,size_t sn){
	if(so == NULL && sn == 0){
		bitch("Requested no-op for %s\n",name);
		return -1;
	}
	if(!!o != !!so || !!n != !!sn){
		bitch("Invalid pairings for %s: %d %d %d %d\n",
				name,!!o,!!so,!!n,!!sn);
		return -1;
	}
	if(sysctlbyname(name,o,so,n,sn)){
		moan("Couldn't %s sysctl %s by name\n",
			so ? sn ? "get+set" : "get" : "set",name);
		return -1;
	}
	return 0;
}
#endif

int Mount(const char *target,const char *fstype,int flags,void *params){
	int ret;

#ifdef LIB_COMPAT_FREEBSD
	if( (ret = mount(target,fstype,flags,params)) ){
		moan("Couldn't mount %s,%s,%d at %s\n",fstype,(const char *)params,flags,target);
	}
#else
#ifdef LIB_COMPAT_LINUX
	if( (ret = mount(NULL,target,fstype,flags,params)) ){
		moan("Couldn't mount %s,%s,%d at %s\n",fstype,(const char *)params,flags,target);
	}
#else
#error "Filesystem mounting is unsupported on this OS."
#endif
#endif
	return ret;
}

int Sendmsg(int sd,const struct msghdr *msg,int flags){
	int ret;

	if((ret = sendmsg(sd,msg,flags)) < 0){
		moan("Couldn't sendmsg() on %d\n",sd);
	}
	return ret;
}

int Sendto(int sd,const void *buf,size_t len,int flags,
		const struct sockaddr *to,socklen_t tolen){
	int ret;

	if((ret = sendto(sd,buf,len,flags,to,tolen)) < 0){
		moan("Couldn't sendto() %zu bytes on %d\n",len,sd);
	}
	return ret;
}
