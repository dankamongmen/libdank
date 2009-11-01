#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <libdank/version.h>
#include <crosier/libcrosier.h>

static int
send_fd(int sd,int fd,void *data,size_t s){
	char buf[CMSG_SPACE(sizeof(int))];
	struct iovec iov[1];
	struct cmsghdr *cmsg;
	struct msghdr mh;

	if(s > INT_MAX){
		fprintf(stderr,"Auxillary data too large (%zu bytes)\n",s);
		return -1;
	}
	memset(&mh,0,sizeof(mh));
	iov[0].iov_base = data;
	iov[0].iov_len = s;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_control = buf;
	mh.msg_controllen = sizeof(buf);

	cmsg = CMSG_FIRSTHDR(&mh);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg),&fd,sizeof(fd));
	mh.msg_controllen = cmsg->cmsg_len;

	return sendmsg(sd,&mh,0) == (int)s ? 0 : -1;
}

int send_ctlrequest(int sd,const char *cmd,FILE *fp){
	size_t ret;
	char *buf;

	if(write(sd,cmd,strlen(cmd) + 1) != (ssize_t)strlen(cmd) + 1){
		printf("Error writing cmd \"%s\": %s\n",cmd,strerror(errno));
		return -1;
	}
	if((buf = malloc(BUFSIZ)) == NULL){
		fprintf(stderr,"Couldn't allocate read buffer\n");
		return -1;
	}
	while((ret = fread(buf,1,BUFSIZ,fp)) > 0){
		if(write(sd,buf,ret) < (ssize_t)ret){
			printf("Error writing %zu bytes: %s\n",ret,strerror(errno));
			free(buf);
			return -1;
		}
	}
	free(buf);
	if(ferror(fp)){
		printf("Error reading command data: %s\n",strerror(errno));
		return -1;
	}
	if(shutdown(sd,SHUT_WR)){
		printf("Error shutting down send: %s\n",strerror(errno));
		return -1;
	}
	return 0;	
}

int send_ctlrequest_buf(int sd,const char *cmd,const char *buf){
	size_t buflen;

	if(write(sd,cmd,strlen(cmd) + 1) != (ssize_t)strlen(cmd) + 1){
		printf("Error writing cmd \"%s\": %s\n",cmd,strerror(errno));
		return -1;
	}
	buflen = strlen(buf);
	while(buflen){
		ssize_t tosend = buflen > BUFSIZ ? BUFSIZ : buflen;

		if(write(sd,buf,tosend) < tosend){
			printf("Error writing %zd bytes: %s\n",tosend,strerror(errno));
			return -1;
		}
		buflen -= tosend;
		buf += tosend;
	}
	if(shutdown(sd,SHUT_WR)){
		printf("Error shutting down send: %s\n",strerror(errno));
		return -1;
	}
	return 0;
}

int recv_ctlreply(int sd){
	ssize_t ret,t = 0;
	char buf[128];

	while((ret = read(sd,buf,sizeof(buf))) > 0){
		size_t nb = ret;

		t += ret;
		if(fwrite(buf,1,nb,stdout) != nb){
			printf("Writing %zu bytes to stdout: %s\n",
					nb,strerror(errno));
			return -1;
		}
	}
	return ret < 0 ? ret : t;
}

int crosier_connect(const char *path,int errfd){
	struct sockaddr_un suna;
	uint32_t version;
	long fdflags;
	int sd;

	if((fdflags = fcntl(errfd,F_GETFD)) < 0 || fcntl(errfd,F_SETFD,fdflags | FD_CLOEXEC)){
		perror("fcntl F_CLOEXEC");
		return -1;
	}
	if((sd = socket(PF_UNIX,SOCK_STREAM,0)) < 0){
		perror("socket(PF_UNIX, SOCK_STREAM, 0)");
		return -1;
	}
#ifdef __BSD_VISIBLE
	{
		int connwait = 1;

		if(setsockopt(sd,SOL_SOCKET,LOCAL_CONNWAIT,&connwait,sizeof(connwait))){
			perror("setsockopt(%d, SOL_SOCKET, LOCAL_CONNWAIT)");
			goto err;
		}
	}
#else
#ifdef LOCAL_CONNWAIT
#warning "LOCAL_CONNWAIT defined, but not __BSD_VISIBLE...I'm confused!"
#endif
#endif
	memset(&suna,0,sizeof(suna));
	suna.sun_family = AF_UNIX;
	strncpy(suna.sun_path,path,sizeof(suna.sun_path) - 1);
	if(connect(sd,(struct sockaddr *)&suna,sizeof(suna))){
		fprintf(stderr,"connect(%d, { AF_UNIX, %s }): %s\n",
				sd,suna.sun_path,strerror(errno));
		goto err;
	}
	version = htonl(CTLSERVER_PROTOCOL_VERSION);
	if(send_fd(sd,errfd,&version,sizeof(version))){
		goto err;
	}
	return sd;

err:
	if(close(sd)){
		fprintf(stderr,"close(%d): %s\n",sd,strerror(errno));
	}
	return -1;
}
