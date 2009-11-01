#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/objustring.h>
#include <libdank/modules/logging/logdir.h>
#include <libdank/modules/logging/logging.h>

// Must only be called within the thread owning this logctx, or after that
// thread has perished. Thus, it is registered with linuxthreads and called
// from the cleanup handler. Standalone apps need not free their logctx's.
void free_logctx(logctx *lc){
	if(lc){
		ustring *out,*err;

		out = lc->out;
		err = lc->err;
		lc->err = lc->out = NULL;
		if(out){
			free_ustring(&out);
		}
		if(err){
			free_ustring(&err);
		}
		if(lc->lfile){
			char buf[80];
			time_t t;

			// we can't use the usual logging in the general case
			// (main thread will still work), because the thread is
			// already gone, and thus get_thread_logctx() fails.
			if((t = time(NULL)) == (time_t)-1 || !ctime_r(&t,buf)){
				strcpy(buf,"\n");
			}
			fprintf(lc->lfile,"%ju|thread ends %s",lc->lineswritten++,buf);
			if(lc->lfile != stdout){
				fclose(lc->lfile);
			}
			lc->lfile = NULL;
			if(lc->cleanup){
				remove(lc->lfile_name);
			}else{
				if(truncate(lc->lfile_name,lc->lfile_offset)){
					remove(lc->lfile_name);
				}
			}
		}
	}
}

static pthread_key_t lfile_key;
static pthread_once_t lfile_key_once = PTHREAD_ONCE_INIT;

static void
free_logctx_wrapper(void *unsafe_lc){
	free_logctx(unsafe_lc);
	deeperfree(unsafe_lc);
}

static void
lfile_key_alloc(void){
	if(pthread_key_create(&lfile_key,free_logctx_wrapper)){
		pthread_exit(NULL);
	}
}

static void
set_logctx_tsd(logctx *lc){
	if(pthread_once(&lfile_key_once,lfile_key_alloc)){
		pthread_exit(NULL);
	}
	if(pthread_setspecific(lfile_key,lc)){
		pthread_exit(NULL);
	}
}

logctx *get_thread_logctx(void){
	return pthread_getspecific(lfile_key);
}

// intitialize with mask and NULL ustrings, file
void init_private_logctx(logctx *lc){
	memset(lc,0,sizeof(*lc));
	set_logctx_tsd(lc);
}

// intitialize a logctx with the provided flags, NULL out/err ustrings,
// and an attempt at a thread-specific logfile. this is suitable for a
// thread or anywhere that a precreated lc is not available, but
// synchronization among all lc's initialized within one thread's
// context is the user's task.
void init_thread_logctx(logctx *lc,const char *tname){
	init_private_logctx(lc);
	lc->lfile = open_thread_log(tname,lc->lfile_name);
}

void init_detached_thread_logctx(logctx *lc,const char *tname){
	init_private_logctx(lc);
	lc->lfile = open_thread_log(tname,lc->lfile_name);
	lc->cleanup = 1;
}

// Create and initialize the provided lc's ->out and ->err buffers. This only
// need be done if the data is to be passed back beyond the logfiles.
int create_logctx_ustrings(void){
	ustring *out,*err;
	logctx *lc;

	if((lc = get_thread_logctx()) == NULL){
		return -1;
	}
	if((out = create_ustring()) == NULL){
		return -1;
	}
	if((err = create_ustring()) == NULL){
		free_ustring(&out);
		return -1;
	}
	lc->out = out;
	lc->err = err;
	return 0;
}

void reset_logctx_ustrings(void){
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		reset_ustring(lc->out);
		reset_ustring(lc->err);
	}
}

void free_logctx_ustrings(void){
	ustring *out,*err;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		out = lc->out;
		err = lc->err;
		lc->out = lc->err = NULL;
		free_ustring(&out);
		free_ustring(&err);
	}
}
