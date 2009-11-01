#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

static inline int
make_tcp_socket(int pf){
	return Socket(pf,SOCK_STREAM,0);
}

int make_nblock_tcp_socket(void){
	int sd;

	if((sd = make_tcp_socket(PF_INET)) < 0){
		return -1;
	}
	if(set_fd_nonblocking(sd)){
		Close(sd);
		return -1;
	}
	return sd;
}

int make_nlinger_tcp_socket(void){
	static struct linger linger = {1,	0,	};
	int sd;

	if((sd = make_tcp_socket(PF_INET)) < 0){
		return -1;
	}
	if(Setsockopt(sd,SOL_SOCKET,SO_LINGER,&linger,sizeof(linger))){
		Close(sd);
		return -1;
	}
	return sd;
}

int make_nblock_nlinger_tcp_socket(void){
	static struct linger linger = {1,	0,	};
	int sd;

	if((sd = make_nblock_tcp_socket()) < 0){
		return -1;
	}
	if(Setsockopt(sd,SOL_SOCKET,SO_LINGER,&linger,sizeof(linger))){
		Close(sd);
		return -1;
	}
	return sd;
}

int make_listener(const struct sockaddr *sa,socklen_t slen,int bufsz){
	int sd,reuse = 1;

	if((sd = make_tcp_socket(sa->sa_family)) < 0){
		return -1;
	}
	if(Setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))){
		Close(sd);
		return -1;
	}
	if(bufsz){
		if(set_socket_rcvbuf(sd,bufsz)){
			return -1;
		}
		if(set_socket_sndbuf(sd,bufsz)){
			return -1;
		}
	}
	// See RFC 4038, 6.3.1, "Example of TCP Server Application". This is
	// necessary to sort out the port space. FreeBSD will only listen on
	// IPv6, but Linux will map IPv4 there -- and thus refuse an IPv4
	// wildcard bind to the same port without IPV6_V6ONLY. Furthermore, the
	// FreeBSD situation changes based on the net.inet6.ip6.v6only sysctl.
	if(sa->sa_family == AF_INET6){
		if(Setsockopt(sd,IPPROTO_IPV6,IPV6_V6ONLY,(char *)&reuse,sizeof(reuse))){
			Close(sd);
			return -1;
		}
	}
	if(Bind(sd,sa,slen)){
		Close(sd);
		return -1;
	}
	if(Listen(sd)){
		Close(sd);
		return -1;
	}
	return sd;
}

int make_listening_sd(const struct in6_addr *ip,unsigned port,int bufsz){
	struct sockaddr_in6 sain;
	int sd;

	if(port > 65535){
		bitch("Invalid TCPv6 port: %u\n",port);
		return -1;
	}
	memset(&sain,0,sizeof(sain));
	sain.sin6_family = AF_INET6;
	memcpy(&sain.sin6_addr.s6_addr,ip,sizeof(*ip));
	sain.sin6_port = port;
	if((sd = make_listener((const struct sockaddr *)&sain,sizeof(sain),bufsz)) >= 0){
		nag("Listening on tcp/%hu with sd %d\n",ntohs(port),sd);
	}
	return sd;
}

int make_listening4_sd(const struct in_addr *ip,unsigned port,int bufsz){
	struct sockaddr_in sain;
	int sd;

	if(port > 65535){
		bitch("Invalid TCPv4 port: %u\n",port);
		return -1;
	}
	memset(&sain,0,sizeof(sain));
	sain.sin_family = AF_INET;
	memcpy(&sain.sin_addr.s_addr,ip,sizeof(*ip));
	sain.sin_port = port;
	if((sd = make_listener((const struct sockaddr *)&sain,sizeof(sain),bufsz)) >= 0){
		nag("Listening on tcp/%hu with sd %d\n",ntohs(port),sd);
	}
	return sd;
}

// Decodes socket errors with getsockopt(), returning != 0 if there were any.
int log_socket_errors(int sd){
	socklen_t errlen;
	int err = 0;

	errlen = sizeof(err);
	if(Getsockopt(sd,SOL_SOCKET,SO_ERROR,&err,&errlen)){
		return -1;
	}
	if(errlen != sizeof(int)){
		bitch("Malformed return value from getsockopt on %d\n",sd);
		return -1;
	}
	if(err){
		errno = err;
		moan("Socket operation failed on %d\n",sd);
		return -1;
	}
	return 0;
}

static int
suck_sd_line(int sd,char *buffer,size_t s){
	size_t len = 0;
	int r = 0;

	while(len < s){
		if((r = read(sd,buffer + len,1)) == 0){
			buffer[len] = '\0';
			return len;
		}else if(r < 0){
			if(errno == EINTR){
				continue;
			}
			moan("Couldn't read from socket %d.\n",sd);
			return -1;
		}
		if(buffer[len] == '\n' || buffer[len] == '\0'){
			buffer[len] = '\0';
			return len;
		}
		++len;
	}
	bitch("Input larger than %zu-byte buffer.\n",s);
	return -1;
}

int read_socket_dynline(int sd,char **buf){
	size_t len = 0;
	int ret;

	*buf = NULL;
	do{
		char *tmp;
		int err;

		len += BUFSIZ;
		if((tmp = Realloc("socket line",*buf,len)) == NULL){
			Free(*buf);
			*buf = NULL;
			return -1;
		}
		*buf = tmp;
		errno = 0;
		if((ret = suck_sd_line(sd,*buf + len - BUFSIZ,BUFSIZ)) >= 0){
			err = errno;
			nag("Copied %d bytes from socket\n",ret);
			errno = err;
		}
	}while(ret < 0 && errno == 0); // allocate more
	if(ret <= 0){
		Free(*buf);
		*buf = NULL;
		return -1;
	}
	return 0;
}

int is_sock_listening(int fd){
	socklen_t slen;
	int res;

	slen = sizeof(res);
	if(Getsockopt(fd,SOL_SOCKET,SO_ACCEPTCONN,&res,&slen)){
		return -1;
	}
	return res;
}

// http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=308341
int get_socket_rcvbuf(int sd,int *size){
	socklen_t slen;

	slen = sizeof(*size);
	return Getsockopt(sd,SOL_SOCKET,SO_RCVBUF,size,&slen);
}

int get_socket_sndbuf(int sd,int *size){
	socklen_t slen;

	slen = sizeof(*size);
	return Getsockopt(sd,SOL_SOCKET,SO_SNDBUF,size,&slen);
}

int set_socket_rcvbuf(int sd,int size){
	return Setsockopt(sd,SOL_SOCKET,SO_RCVBUF,&size,sizeof(size));
}

int set_socket_sndbuf(int sd,int size){
	return Setsockopt(sd,SOL_SOCKET,SO_SNDBUF,&size,sizeof(size));
}
