#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/tracing/oops.h>
#include <libdank/modules/logging/logdir.h>
#include <libdank/modules/logging/health.h>
#include <libdank/modules/logging/logging.h>
#include <libdank/modules/ctlserver/ctlserver.h>

#define KBYTE_SIZE 1024
#define LOG_KBYTES 1024
#define MSG_QUEUE_SZ 1024 // backlog before dropping msgs

typedef struct listener_msg {
	char msg[LISTENER_MSG_SZ];
	int len;
} listener_msg;

typedef struct listener {
	int dropped;
	unsigned head,tail;
	pthread_cond_t cond;
	pthread_mutex_t lock;
	struct listener *next;
	listener_msg msg_queue[MSG_QUEUE_SZ],*lm;
} listener;

static const off_t LOGFILE_MAX_SIZE = KBYTE_SIZE * LOG_KBYTES;

static inline unsigned
inc_qindex(unsigned i){
	return (i + 1) % MSG_QUEUE_SZ;
}

static listener *listeners;
static unsigned prohibit_new_listeners = 1;
static pthread_mutex_t listener_lock = PTHREAD_MUTEX_INITIALIZER;

// Returns zero if l->lm is a valid listener_msg, non-zero if the thread should
// exit (the log stream has closed). Blocks until one of these is true.
static int
block_on_lmsg(listener *l){
	pthread_mutex_lock(&l->lock);
	if(l->lm){
		if(l->dropped){
			int drops = l->dropped;

			l->dropped = 0;
			l->lm->len = snprintf(l->lm->msg,LISTENER_MSG_SZ,
					"%d messages dropped\n",drops);
			pthread_mutex_unlock(&l->lock);
			return 0;
		}else{
			l->lm = NULL;
		}
	}
	while(l->head == l->tail){
		pthread_cond_wait(&l->cond,&l->lock);
	}
	l->lm = &l->msg_queue[l->head];
	l->head = inc_qindex(l->head);
	pthread_mutex_unlock(&l->lock);
	return !l->lm->len;
}

static void
free_listener(listener **l){
	if(l && *l){
		Pthread_mutex_destroy(&(*l)->lock);
		Pthread_cond_destroy(&(*l)->cond);
		Free(*l);
		*l = NULL;
	}
}

static listener *
create_listener(void){
	listener *ret;

	if( (ret = Malloc("listener",sizeof(*ret))) ){
		memset(ret,0,sizeof(*ret));
		if(pthread_mutex_init(&ret->lock,NULL)){
			Free(ret);
			return NULL;
		}
		if(pthread_cond_init(&ret->cond,NULL)){
			Pthread_mutex_destroy(&ret->lock);
			Free(ret);
			return NULL;
		}
		pthread_mutex_lock(&listener_lock);
			if(prohibit_new_listeners == 0){
				ret->next = listeners;
				listeners = ret;
			}else{
				free_listener(&ret);
			}
		pthread_mutex_unlock(&listener_lock);
	}
	return ret;
}

static void
purge_listener(listener **l){
	if(l && *l){
		pthread_mutex_lock(&listener_lock);{
			listener **prev = &listeners;

			while(*prev){
				if(*prev == *l){
					*prev = (*l)->next;
					break;
				}
				prev = &(*prev)->next;
			}
		}pthread_mutex_unlock(&listener_lock);
		free_listener(l);
	}
}

static int
attach_listener(int sd){
	listener *l;

	if(sd < 0){
		return -1;
	}
	if((l = create_listener()) == NULL){
		return -1;
	}
	do{
		if(block_on_lmsg(l)){
#define DIEMSG "Uh-oh, I'm a logdumper! What a long, strange trip it's been...\n"
			Writen(sd,DIEMSG,strlen(DIEMSG));
			break;
#undef DIEMSG
		}
	}while(Writen(sd,l->lm->msg,(size_t)l->lm->len) == 0);
	purge_listener(&l);
	return 0;
}

static void
enqueue_msg(listener *l,const char *msg,int len){
	listener_msg *lm;
	int wakeup = 1;

	if(pthread_mutex_trylock(&l->lock)){
		return; // Any contention will result in dropped messages!
	}
	if(inc_qindex(l->tail) != l->head){
		lm = &l->msg_queue[l->tail];
		l->tail = inc_qindex(l->tail);

		lm->len = len;
		// We rely on the entry point to this callchain (inner_vflog())
		// to ensure this is safe...
		strcpy(lm->msg,msg);
	}else{
		l->dropped++;
		wakeup = 0;
	}
	pthread_mutex_unlock(&l->lock);
	if(wakeup){
		pthread_cond_signal(&l->cond);
	}
}

static inline void
enqueue_all(const char *msg,int len){
	listener *l;

	for(l = listeners ; l ; l = l->next){
		// FIXME if this is the magic ("", 0) sentinel for shutdown, we
		// can't afford to drop the message...
		enqueue_msg(l,msg,len);
	}
}

static void
listener_log(const char *msg,int len){
	// We've taken listener_log and enqueue_all and turned parts of them
	// inside out to avoid stack allocation of the message.  Since
	// vsnprintf will not cause any listener activity, we can safely
	// move it inside the listener_lock, and that allows us to use a
	// file-local static buffer.
	if(pthread_mutex_trylock(&listener_lock)){
		return;
	}
	enqueue_all(msg,len);
	pthread_mutex_unlock(&listener_lock);
}

// concession to timevflog(), and the usefulness of printing a single leader
static void
inner_vflog(const char *leader,const char *fmt,va_list args){
	size_t avail; // available for chars, not chars + null
	logctx *lc;
	int len,z;

	if((lc = get_thread_logctx()) == NULL){
		// fall back onto stdio -- need to distinguish FILE *s FIXME
		vfprintf(stdout,fmt,args);
		return;
	}
	avail = sizeof(lc->msg_buffer) - 1;
	if((len = snprintf(lc->msg_buffer,avail,"%ju|",lc->lineswritten++)) < 0 || (size_t)len >= avail){
		return;
	}
	avail -= len;
	z = leader ? strlen(leader) : 0;
	if(z){
		if((size_t)z < avail){
			memcpy(lc->msg_buffer + len,leader,z + 1);
		}else{
			z = 0;
		}
	}
	avail -= z;
	len += z;
	if((z = vsnprintf(lc->msg_buffer + len,avail,fmt,args)) < 0 || ((size_t)z >= avail)){
		len = sizeof(lc->msg_buffer) - 1;
		lc->msg_buffer[len - 1] = '\n';
	}else{
		len += z;
	}
	listener_log(lc->msg_buffer,len);
	if(lc->lfile){
		int ret;

		if((ret = fwrite(lc->msg_buffer,1,(size_t)len,lc->lfile)) < len){
			lc->lfile = NULL;
		}else if(lc->lfile != stdout){
			lc->lfile_offset += ret;
			if(lc->lfile_offset > LOGFILE_MAX_SIZE){
				if(ftruncate(fileno(lc->lfile),LOGFILE_MAX_SIZE)){
					lc->lfile = NULL;
				}else{
					rewind(lc->lfile);
					lc->lfile_offset = 0;
				}
			}
		}
	}
}
	
void vflog(const char *fmt,va_list args){
	inner_vflog(NULL,fmt,args);
}

void flog(const char *fmt,...){
	va_list ap;

	va_start(ap,fmt);
	vflog(fmt,ap);
	va_end(ap);
}

static inline void
timevflog(const char *fmt,va_list args){
	char buf[80];
	time_t t;

	buf[0] = '\0';
	if((t = time(NULL)) != (time_t)-1){
		if(ctime_r(&t,buf)){
			char *newline;

			if( (newline = strchr(buf,'\n')) ){
				*newline++ = ' ';
				*newline = '\0';
			}
		}
	}
	inner_vflog(buf,fmt,args);
}

void timeflog(const char *fmt,...){
	va_list ap;

	va_start(ap,fmt);
	timevflog(fmt,ap);
	va_end(ap);
}

// Initialize the logging system. This should be the first order of business
// for each application after setting the desired umask, and indeed before
// setting up fatal signal handlers.
//
// The logdir argument will be used if non-NULL (mkdir() will be used if
// necessary, but not recursively), and the current directory otherwise.
// open_thread_log() is called on the lc's behalf, using logdir. We try to
// reserve the file "crash", allocating it the maximum space available to a
// logfile (all are length-capped). This is used by fatal signal handlers.
int init_logging(logctx *lc,const char *logdir,int stdio){
	init_private_logctx(lc);
	if(stdio){
		if(set_log_stdio()){
			return -1;
		}
	}else{
		if(logdir){
			if(set_logdir(logdir)){
				return -1;
			}
		}else{
			if(create_logctx_ustrings()){
				return -1;
			}
		}
	}
	return 0;
}

// Log the provided error code and close down the main logfile. Should be
// called immediately prior to exit() or raise(SIGKILL).
int stop_logging(int retcode){
	track_main("Called stop_logging()");
	truncate_crash_log(retcode);
	return 0;
}

// Switch to the crash log, which has space pre-reserved on disk, after
// (re)initializing the provided lc. May be effectively called only once.
// truncate_crash_log() knows how to cope with a caller no matter the status
// of the crashlog. This isn't strictly threadsafe, but only breaks in a race
// resulting from crashing while truncating the crash log, so write that code
// really well.
void log_crash(logctx *lc){
	get_crash_log(lc);
}

static int
srv_dump_log(cmd_state *cs){
	if(attach_listener(cs->sd)){
		return -1;
	}
	cs->sent_success = 1;
	return 0;
}

static int
srv_mem_dump(cmd_state *cs __attribute__ ((unused))){
	int ret = -1;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		ret = stringize_memory_usage(lc->out);
	}
	return ret;
}

static int
srv_health_dump(cmd_state *cs __attribute__ ((unused))){
	int ret = -1;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		ret = stringize_health(lc->out);
	}
	return ret;
}

static command commands[] = {
	{ .cmd = "log_dump",	.func = srv_dump_log,		},
	{ .cmd = "mem_dump",	.func = srv_mem_dump,		},
	{ .cmd = "health_dump", .func = srv_health_dump,	},
	{ NULL,			NULL,				}
};

// Register the CTLserver commands provided by the logging module.
int init_log_server(void){
	prohibit_new_listeners = 0;
	if(regcommands(commands)){
		prohibit_new_listeners = 1;
		return -1;
	}
	return 0;
}

// Unregister the logging module's CTLserver commands.
int stop_log_server(void){
	int ret = 0;

	nag("Killing all logdumpers\n");
	pthread_mutex_lock(&listener_lock);
		prohibit_new_listeners = 1;
		enqueue_all("",0); // see block_on_lmsg()
	pthread_mutex_unlock(&listener_lock);
	ret |= delcommands(commands);
	return ret;
}
