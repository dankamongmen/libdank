#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/localsock.h>

int connect_local(const char *path){
	struct sockaddr_un su;
	int sd;

	if((sd = Socket(PF_LOCAL,SOCK_STREAM,0)) < 0){
		return -1;
	}
	memset(&su,0,sizeof(su));
	su.sun_family = AF_LOCAL;
	if(strlen(path) >= sizeof(su.sun_path)){
		bitch("Socket address (%s) too long (%zd >= %zd)\n",
				path,strlen(path),sizeof(su.sun_path));
		Close(sd);
		return -1;
	}
	strcpy(su.sun_path,path);
	if(Connect(sd,(struct sockaddr *)&su,SUN_LEN(&su))){
		Close(sd);
		return -1;
	}
	return sd;
}

static int
listen_local_common(int sd,const char *path){
	struct sockaddr_un su;

	// Don't use Unlink(); this file need not exist
	if(unlink(path) < 0 && errno != ENOENT){
		moan("Couldn't unlink %s.\n",path);
		return -1;
	}
	memset(&su,0,sizeof(su));
	su.sun_family = AF_LOCAL;
	if(strlen(path) >= sizeof(su.sun_path)){
		bitch("Socket address (%s) too long (%zd >= %zd)\n",
				path,strlen(path),sizeof(su.sun_path));
		close(sd);
		return -1;
	}
	strcpy(su.sun_path,path);
	// see [ Stevens 92 @ 15.23 ]
	/* su.sun_len = sizeof(su.sun_len) + sizeof(su.sun_family) +
			strlen(su.sun_path) + 1; */
	if(Bind(sd,(struct sockaddr *)&su,SUN_LEN(&su))){
		return -1;
	}
	return 0;
}

int listen_local(const char *path){
	int sd;

	if((sd = Socket(PF_LOCAL,SOCK_STREAM,0)) < 0){
		return -1;
	}
	if(listen_local_common(sd,path)){
		Close(sd);
		return -1;
	}
	if(Listen(sd)){
		Close(sd);
		return -1;
	}
	return sd;
}

int listen_local_dgram(const char *path){
	int sd;

	if((sd = Socket(AF_LOCAL,SOCK_DGRAM,0)) < 0){
		return -1;
	}
	if(listen_local_common(sd,path)){
		Close(sd);
		return -1;
	}
	return sd;
}

int accept_local(int lsd,struct sockaddr_un *sun,int flags){
	struct sockaddr_un lazysun;
	socklen_t slen;
	int sd;

	if(sun == NULL){
		memset(&lazysun,0,sizeof(lazysun));
		sun = &lazysun;
	}
	slen = sizeof(*sun);
	sun->sun_family = AF_UNIX;
	// nag("Accepting PF_UNIX connections on sd %d\n",lsd);
	if((sd = Accept4(lsd,(struct sockaddr *)sun,&slen,flags)) < 0){
		return -1;
	}
	nag("Accepted PF_UNIX conn at %d from %s on %d\n",sd,
		sun->sun_path[0] ? sun->sun_path : "abstract PF_UNIX",lsd);
	return sd;
}
