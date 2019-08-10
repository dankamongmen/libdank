#include <stdint.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/string.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/utils/localsock.h>
#include <libdank/modules/events/fds.h>
#include <libdank/modules/tracing/oops.h>
#include <libdank/modules/events/evcore.h>
#include <libdank/modules/tracing/threads.h>
#include <libdank/modules/ctlserver/ctlserver.h>

typedef struct export_cmd {
	unsigned refcount;
	const command *cmd;
	struct export_cmd *next;
} export_cmd;

typedef struct scheduled_cmd {
	cmd_state cs;
	export_cmd *cmd;
	struct scheduled_cmd *next;
} scheduled_cmd;

typedef struct drone {
	pthread_t tid;
} drone;

typedef struct ctlserv_marshal {
	struct sockaddr_un suna;
	char cmdbuf[128]; // FIXME 128?
} ctlserv_marshal;

#define DRONE_HORDE_SIZE 3

static char *listensn;
static evhandler *ctlev;
static int listenfd = -1;
static export_cmd *cmdtable;
static ctlserv_marshal *cmarsh;
static const command server_commands[];
static scheduled_cmd *pending_cmd_states;
static drone drone_horde[DRONE_HORDE_SIZE];

// Don't use static initializers, as it fouls up multiple-run unit testing (or
// anything else which would start and stop ctlserver instances).
static pthread_cond_t srvrcond;
static pthread_mutex_t srvrlock;

static int
mainserv_close(void){
	int ret = 0;

	if(listenfd >= 0){
		ret |= Unlink(listensn);
		ret |= Close(listenfd);
		listenfd = -1;
	}
	// We're guaranteed listensn is NULL unless we've initialized it, so
	// just go ahead and free() + reset it.
	Free(listensn);
	listensn = NULL;
	return ret;
}

static int
delcommand(const char *cmd){
	export_cmd *cur,**pre;
	unsigned refcount = 0;

	do{
		pthread_mutex_lock(&srvrlock);
		for(pre = &cmdtable ; (cur = *pre) ; pre = &cur->next){
			if(strcmp(cmd,cur->cmd->cmd) == 0){
				if((refcount = cur->refcount) == 0){
					*pre = cur->next;
				}
				break;
			}
		}
		pthread_mutex_unlock(&srvrlock);
		if(!cur){
			bitch("Couldn't find ctlserver entry for %s\n",cmd);
			return -1;
		}
		// FIXME don't busy-wait; use a cond...
	}while(refcount);
	Free(cur); // cur has been extracted, we needn't have the lock
	nag("Eliminated ctlserver entry for %s\n",cmd);
	return 0;
}

int delcommands(const command *cmd){
	int ret = 0;

	if(cmd){
		while(cmd->cmd){
			ret |= delcommand(cmd->cmd);
			++cmd;
		}
	}
	return ret;
}

static int
regcommand(const command *cmd){
	export_cmd *tmp;

	if((tmp = Malloc("ctlserv handler",sizeof(*tmp))) == NULL){
		return -1;
	}
	tmp->cmd = cmd;
	tmp->refcount = 0;
	pthread_mutex_lock(&srvrlock);
		tmp->next = cmdtable;
		cmdtable = tmp;
	pthread_mutex_unlock(&srvrlock);
	return 0;
}

int regcommands(const command *cmd){
	int i = 0;

	while(cmd->cmd){
		if(regcommand(cmd) < 0){
			while(i--){
				delcommand((--cmd)->cmd);
			}
			return -1;
		}
		++i;
		++cmd;
	}
	return 0;
}

static int
server_shutdown(cmd_state *cs __attribute__ ((unused))){
	int sig = SIGTERM;

	// Either the signal will immediately terminate us, or we've registered
	// a handler, or a thread is sigwait()ing on the signal (which
	// contraindicates signal handlers). If a thread is sigwait()ing, on a
	// signal with process-wide semantics (ie, delivered to all threads),
	// all other threads must have the signal blocked anyway (this is
	// feasible due to signal masks being inherited on pthread_create()),
	// so sending the signal to the process works perfectly without having
	// to store the target TID (which we might not even know).
	nag("Executing shutdown: signal %d (%s)\n",sig,strsignal(sig));
	if(kill(getpid(),sig)){
		return -1;
	}
	return 0;
}

static int
stringize_help_locked(ustring *u){
	const typeof(*server_commands) *scur;
	const typeof(*cmdtable) *cur;

	for(scur = server_commands ; scur->cmd ; ++scur){
		if(printUString(u,"internal] %s\n",scur->cmd) < 0){
			return -1;
		}
	}
	for(cur = cmdtable ; cur ; cur = cur->next){
		if(printUString(u,"external] %s\n",cur->cmd->cmd) < 0){
			return -1;
		}
	}
	return 0;
}

static int
server_help(cmd_state *cs __attribute__ ((unused))){
	return dump_lock(stringize_help_locked,&srvrlock);
}

static int
server_noop(cmd_state *cs __attribute__ ((unused))){
	nag("No operation here\n");
	return 0;
}

static void
ctldrone_main(void *v __attribute__ ((unused))){
	logctx *lc;

	if((lc = get_thread_logctx()) == NULL){
		return;
	}
	if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL)){
		bitch("Couldn't become cancellable\n");
		return;
	}
	while(1){
		ustring out = USTRING_INITIALIZER,err = USTRING_INITIALIZER;
		ustring *oldout,*olderr;
		scheduled_cmd *me;
		cmd_state *cs;
		int oldstate;

		PTHREAD_PUSH(&srvrlock,cleanup_mutex);
		while((me = pending_cmd_states) == NULL){
			if(pthread_cond_wait(&srvrcond,&srvrlock)){
				bitch("Couldn't wait on condvar %p/%p\n",&srvrcond,&srvrlock);
			}
		}
		pending_cmd_states = me->next;
		PTHREAD_POP();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&oldstate);
		oldout = lc->out;
		lc->out = &out;
		olderr = lc->err;
		lc->err = &err;
		cs = &me->cs;
		timenag("%s on %d/%d\n",me->cmd->cmd->cmd,cs->sd,cs->errsd);
		me->cmd->cmd->func(cs);
		if(!cs->sent_success){
			nag("%zu outb on %d, %zu errb on %d\n",lc->out->current,cs->sd,
					lc->err->current,cs->errsd);
			if(lc->out->current){
				Writen(cs->sd,lc->out->string,lc->out->current);
			}
			if(lc->err->current){
				Writen(cs->errsd,lc->err->string,lc->err->current);
			}
		}
		pthread_mutex_lock(&srvrlock);
		--me->cmd->refcount;
		pthread_mutex_unlock(&srvrlock);
		close_cmd_state(cs);
		Free(me);
		reset_ustring(&out);
		lc->out = oldout;
		reset_ustring(&err);
		lc->err = olderr;
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
		pthread_testcancel();
	}
}

static int
schedule(int sd,int errsd,const char *cmdstr){
	static const char ERR_NO_RESOURCES[] = "No resources for command.\n";
	static const char ERR_NO_HANDLER[] = "No handler for command.\n";
	scheduled_cmd *cmd = NULL;
	export_cmd *cur;

	PTHREAD_LOCK(&srvrlock);
	for(cur = cmdtable ; cur ; cur = cur->next){
		if(strcmp(cur->cmd->cmd,cmdstr) == 0){
			if( (cmd = Malloc("ctldrone",sizeof(*cmd))) ){
				init_cmd_state(&cmd->cs,sd,errsd);
				++cur->refcount;
				cmd->cmd = cur;
				cmd->next = pending_cmd_states;
				pending_cmd_states = cmd;
			}
			break;
		}
	}
	PTHREAD_UNLOCK(&srvrlock);
	pthread_cond_signal(&srvrcond);
	if(cur == NULL){
		Writen(errsd,ERR_NO_HANDLER,strlen(ERR_NO_HANDLER));
	}else if(cmd == NULL){
		Writen(errsd,ERR_NO_RESOURCES,strlen(ERR_NO_RESOURCES));
	}else{ // drone or shutdown process will clean up cmd state (and sd's)
		return 0;
	}
	bitch("Couldn't schedule command %s; sent error\n",cmdstr);
	Close(sd);
	Close(errsd);
	return -1;
}

// On Linux 2.6.23 and greater, MSG_CMSG_CLOEXEC atomically sets the
// close-on-exec flag for file descriptors passed in SCM_RIGHTS messages.
// Otherwise, other threads calling fork(2)+exec(2) can race against the
// fcntl() operation, and bleed file descriptors across exec(2)s. On older
// Linux and all FreeBSD, this fundamental race exists!
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif

// reads up to *sz - 1
static int
recv_fd(int sd,char *data,size_t *sz){
	char buf[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	struct msghdr mh;
	uint32_t version;
	int recvd;
	size_t z;

	memset(&mh,0,sizeof(mh));
	iov[0].iov_base = (void *)&version;
	iov[0].iov_len = sizeof(version);
	mh.msg_iov = iov;
	mh.msg_control = buf;
	mh.msg_controllen = sizeof(buf);
	mh.msg_iovlen = sizeof(iov) / sizeof(*iov);

	// FIXME might want to supply MSG_ERRQUEUE / MSG_WAITALL?
	if((recvd = recvmsg(sd,&mh,MSG_CMSG_CLOEXEC)) < (int)sizeof(version)){
		if(recvd < 0){
			moan("Error reading SCM_RIGHTS message\n");
		}else{
			bitch("Short (%d) read on SCM_RIGHTS message\n",recvd);
		}
		return -1;
	}
	if((cmsg = CMSG_FIRSTHDR(&mh)) == NULL){
		bitch("Malformed SCM_RIGHTS message\n");
		return -1;
	}
	if(cmsg->cmsg_len != CMSG_LEN(sizeof(int))){
		bitch("Badly-sized SCM_RIGHTS message (%zu != %zu)\n",
			CMSG_LEN(sizeof(int)),(size_t)cmsg->cmsg_len);
		return -1;
	}
	if(cmsg->cmsg_level != SOL_SOCKET){
		bitch("Bad level receiving fd (%d)\n",cmsg->cmsg_level);
		return -1;
	}
	if(cmsg->cmsg_type != SCM_RIGHTS){
		bitch("Bad type receiving fd (%d)\n",cmsg->cmsg_type);
		return -1;
	}
	iov[0].iov_len = 1;
	mh.msg_control = NULL;
	mh.msg_controllen = 0;
	for(z = 0 ; z < *sz ; ++z){
		iov[0].iov_base = data + z;
		if((recvd = recvmsg(sd,&mh,0)) < 1){
			if(recvd < 0){
				moan("Error reading command\n");
			}else{
				bitch("Short (%d) read on command\n",recvd);
			}
			return -1;
		}
		if(data[z] == '\0'){
			break;
		}
	}
	*sz = z;
	return *CMSG_DATA(cmsg);
}

static int
ctlserv_accept(int sd,char *cmdbuf,size_t *sz){
	int fd;

	if(*sz < 2){
		bitch("Buffer too small (%zub)\n",*sz);
		return -1;
	}
	// reads up to *sz - 1, stores amount read back in *sz...
	if((fd = recv_fd(sd,cmdbuf,sz)) < 0){
		return -1;
	}
	cmdbuf[*sz] = '\0';
	// ... and thus this is safe (see comment above)
	return fd;
}

// returns 0 if it was a server command, < 0 otherwise
static int
check_for_server_command(int sd,int errsd,const char *cmd){
	const command *c;
	cmd_state cs;

	init_cmd_state(&cs,sd,errsd);
	for(c = server_commands ; c->cmd ; ++c){
		if(strcmp(cmd,c->cmd) == 0){
			c->func(&cs);
			close_cmd_state(&cs);
			return 0;
		}
	}
	return -1;
}

static void
localaccept(int lsd,void *vstate){
	ctlserv_marshal *cm = vstate;
	int sd,errsd;

	while((sd = accept_local(lsd,&cm->suna,LIBDANK_FD_CLOEXEC)) >= 0){
		size_t sz = sizeof(cm->cmdbuf);

		// FIXME this can block!
		if((errsd = ctlserv_accept(sd,cm->cmdbuf,&sz)) < 0){
			Close(sd);
			return;
		}
		if(set_fd_close_on_exec(errsd)){
			Close(sd);
			Close(errsd);
			return;
		}
		timenag("[%s] sd %d, errsd %d\n",cm->cmdbuf,sd,errsd);
		// If this is a server command, handle it in our context.
		if(check_for_server_command(sd,errsd,cm->cmdbuf) == 0){
			return;
		}
		if(schedule(sd,errsd,cm->cmdbuf)){
			return;
		}
	}
}

int dump_lock(stringizer sfxn,pthread_mutex_t *mtx){
	int ret;

	PTHREAD_PUSH(mtx,cleanup_mutex);
	ret = dump(sfxn);
	PTHREAD_POP();
	return ret;
}

static const command server_commands[] = {
	{"shutdown",		server_shutdown,	},
	{"internal_noop",	server_noop,		},
	{NULL,			NULL,			}
};

static const command commands[] = {
	{"help",		server_help,		},
	{"external_noop",	server_noop,		},
	{NULL,			NULL,			}
};

static int
corral_the_herd(const drone *drones,unsigned count){
	int ret = 0;

	while(count){
		ret |= reap_traceable_thread("drone",drones[--count].tid,NULL);
	}
	return ret;
}

static int
sew_dragon_teeth(drone *drones,unsigned count){
	unsigned z;

	nag("Creating %u drones\n",count);
	for(z = 0 ; z < count ; ++z){
		if(new_traceable_thread("ctldrone",&drones[z].tid,ctldrone_main,NULL)){
			corral_the_herd(drones,z);
			return -1;
		}
	}
	return 0;
}

// Preconditions: listensn == NULL, listenfd < 0
int init_ctlserver(const char *sn){
	if((cmarsh = Malloc("ctlserv_marshal",sizeof(*cmarsh))) == NULL){
		goto err;
	}
	if((listensn = Strdup(sn)) == NULL){
		goto err;
	}
	if(Pthread_mutex_init(&srvrlock,NULL)){
		goto err;
	}
	if(Pthread_cond_init(&srvrcond,NULL)){
		goto lockerr;
	}
	if(sew_dragon_teeth(drone_horde,sizeof(drone_horde) / sizeof(*drone_horde))){
		goto conderr;
	}
	// FIXME we should move this inside the ctlserver_main, and use
	// rigorous I/O utils. update doc/ctlserver if we ever do.
	nag("ctlsock at %s\n",sn);
	if((listenfd = listen_local(sn)) < 0){
		goto herderr;
	}
	if(set_fd_close_on_exec(listenfd) || set_fd_nonblocking(listenfd)){
		goto herderr;
	}
	if(regcommands(commands)){
		goto herderr;
	}
	nag("Creating ctl thread for sd %d\n",listenfd);
	if((ctlev = create_evthread(LIBDANK_FD_CLOEXEC)) == NULL){
		goto cmderr;
	}
	if(add_fd_to_evhandler(ctlev,listenfd,localaccept,NULL,cmarsh)){
		goto cmderr;
	}
	return 0;

cmderr:
	delcommands(commands);
herderr:
	corral_the_herd(drone_horde,sizeof(drone_horde) / sizeof(*drone_horde));
conderr:
	Pthread_cond_destroy(&srvrcond);
lockerr:
	Pthread_mutex_destroy(&srvrlock);
err:
	mainserv_close();
	destroy_evhandler(ctlev);
	ctlev = NULL;
	Free(cmarsh);
	return -1;
}

int stop_ctlserver(void){
	int ret = 0;

	if(!ctlev){
		bitch("Ctlserver not running\n");
		return -1;
	}
	ret |= delcommands(commands);
	ret |= mainserv_close();
	ret |= destroy_evhandler(ctlev);
	ret |= corral_the_herd(drone_horde,sizeof(drone_horde) / sizeof(*drone_horde));
	timenag("Killed ctlserver\n");
	ret |= Pthread_mutex_destroy(&srvrlock);
	ret |= Pthread_cond_destroy(&srvrcond);
	Free(cmarsh);
	return ret;
}

int dump(stringizer sfxn){
	int ret = -1;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		ret = sfxn(lc->out);
	}
	return ret;
}
