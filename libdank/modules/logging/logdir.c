#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libdank/utils/fds.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/logging/logdir.h>	

#define KBYTE_SIZE 1024
#define LOG_KBYTES 1024

static char *logdir;
static int use_stdio;
static size_t logdir_avail;
static char crash_fn[PATH_MAX];
static FILE *crash_log,*avail_crash_log;
static pthread_mutex_t logdir_lock = PTHREAD_MUTEX_INITIALIZER;

static FILE *
open_logfile(const char *fn){
	FILE *ret;

	if( (ret = fopen(fn,"wb")) ){
		int fd;

		setlinebuf(ret);
		if((fd = Fileno(ret)) < 0){
			fclose(ret);
			ret = NULL;
		}else if(set_fd_close_on_exec(fd)){
			fclose(ret);
			ret = NULL;
		}
		// We don't necessarily want the performance hit from
		// unbuffered logging...maybe make it an option somehow? FIXME
		/*else if(set_fd_nonblocking(fd)){
			fclose(ret);
			ret = NULL;
		}*/
	}
	return ret;
}
	
static int
reserve_crash_log(const char *fn){
	int i,ret = -1;
	char *kbyte;
	FILE *fp;

	if(crash_log){
		return -1;
	}
	if((kbyte = malloc(KBYTE_SIZE)) == NULL){
		fprintf(stderr,"Couldn't allocate %u for crashbuf\n",KBYTE_SIZE);
		return -1;
	}
	memset(kbyte,' ',KBYTE_SIZE);
	if((fp = open_logfile(fn)) == NULL){
		fprintf(stderr,"Writing to \"%s\": %s\n",fn,strerror(errno));
		goto done;
	}
	for(i = 0 ; i < LOG_KBYTES ; ++i){
		if(fwrite(kbyte,KBYTE_SIZE,1,fp) != 1){
			fprintf(stderr,"Writing to \"%s\": %s\n",fn,strerror(errno));
			fclose(fp);
			goto done;
		}
	}
	rewind(fp);
	strcpy(crash_fn,fn);
	avail_crash_log = crash_log = fp;
	ret = 0;

done:
	free(kbyte);
	return ret;
}

static int
check_for_dir(scandir_arg d){
	if(strcmp(d->d_name,".") == 0){
		return 0;
	}
	if(strcmp(d->d_name,"..") == 0){
		return 0;
	}
	return 1;
}

static void
clean_logdir(const char *dfn){
	struct dirent **namelist;
	char ffn[PATH_MAX];
	int num,printto;

	if((printto = snprintf(ffn,sizeof(ffn),"%s/",dfn)) >= (ssize_t)sizeof(ffn) || printto < 0){
		fprintf(stderr,"Couldn't remove files from %s\n",dfn);
		return;
	}
	if((num = scandir(dfn,&namelist,check_for_dir,alphasort)) < 0){
		fprintf(stderr,"Couldn't remove files from %s: %s\n",
				dfn,strerror(errno));
		return;
	}
	while(num--){
		snprintf(ffn + printto,sizeof(ffn) - (size_t)printto,"%s",namelist[num]->d_name);
		if(unlink(ffn)){
			fprintf(stderr,"Couldn't remove %s: %s\n",ffn,strerror(errno));
		}
		free(namelist[num]);
	}
	free(namelist);
	return;
}

static int
try_logdir(size_t used){
	size_t avail = logdir_avail - used;
	const char CRASH_LOG[] = "crash";
	char *pos = logdir + used;
	size_t crashsiz;
	int ret = -1;

	crashsiz = strlen(CRASH_LOG) + 1;
	if(crashsiz > avail){
		fprintf(stderr,"Logdir too long: \"%s\",%zu/%zu\n",
				logdir,avail,crashsiz);
		goto reset;
	}
	if(mkdir(logdir,S_IRWXU)){
		if(errno != EEXIST){
			fprintf(stderr,"Error creating logdir \"%s\": %s\n",
					logdir,strerror(errno));
			goto reset;
		}
		clean_logdir(logdir);
	}
	memcpy(pos,CRASH_LOG,crashsiz);
	if(reserve_crash_log(logdir)){
		goto reset;
	}
	logdir_avail = avail;
	ret = 0;

reset:
	logdir[used] = '\0';
	return ret;
}

static FILE *
set_logdir_locked(const char *applogdir,char *fn){
	size_t slen;

	logdir_avail = PATH_MAX + 1;
	if(logdir_avail < strlen(applogdir) + 2){
		fprintf(stderr,"Log directory name too long: %s\n",applogdir);
		return NULL;
	}
	if((logdir = malloc(logdir_avail)) == NULL){
		fprintf(stderr,"Couldn't allocate %zu for logdir\n",logdir_avail);
		return NULL;
	}
	strcpy(logdir,applogdir);
	strcat(logdir,"/");
	slen = strlen(applogdir) + 1;
	if(try_logdir(slen)){
		return NULL;
	}
	return open_thread_log("main",fn);
}

// Indicate that stdio should be used. The logctx passed in have its lfile
// member reset/initialized. Only to be called once, and must be called before
// calling open_logfile() elsewhere, unless set_logdir() is used instead.
int set_log_stdio(void){
	int ret = -1;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		pthread_mutex_lock(&logdir_lock);
		if(!logdir && !use_stdio++){
			lc->lfile = stdout;
			ret = 0;
		}
		pthread_mutex_unlock(&logdir_lock);
	}
	return ret;
}

// Set the logdir. The logctx passed in will have its lfile member
// reset/initialized. Only to be called once, and must be called before calling
// open_logfile() elsewhere. Pass NULL to use the current directory. Sets up a
// logfile with the app's name/PID.
int set_logdir(const char *dir){
	int ret = -1;
	logctx *lc;

	if( (lc = get_thread_logctx()) ){
		pthread_mutex_lock(&logdir_lock);
		if(!logdir && !use_stdio){
			if( (lc->lfile = set_logdir_locked(dir,lc->lfile_name)) ){
				ret = 0;
			}
		}
		pthread_mutex_unlock(&logdir_lock);
	}
	return ret;
}

// supply the name of the thread, returns a logfile in the logdir. the name is saved
// in fn, which should be PATH_MAX + 1 bytes long for k-radness
FILE *open_thread_log(const char *name,char *fn){
	int len = PATH_MAX + 1;
	FILE *ret = NULL;
	pthread_t tid;

	if(logdir == NULL){
		return use_stdio ? stdout : NULL;
	}
	tid = pthread_self();
	if(snprintf(fn,(size_t)len,"%s%s."PRINTF_TIDT,logdir,name,(unsigned long)tid) < len){
		if((ret = open_logfile(fn)) == NULL){
			fn[0] = '\0';
		}
	}
	return ret;
}

static int
get_crash_log_locked(logctx *lc){
	if(avail_crash_log){
		init_private_logctx(lc);
		lc->lfile = crash_log;
		avail_crash_log = NULL;
		return 0;
	}else if(use_stdio){
		init_private_logctx(lc);
		lc->lfile = stdout;
		return 0;
	}
	return -1;
}

void get_crash_log(logctx *lc){
	if(pthread_mutex_trylock(&logdir_lock)){
		return;
	}
	get_crash_log_locked(lc);
	pthread_mutex_unlock(&logdir_lock);
}

static void
truncate_crash_log_locked(int exit_status){
	int fd = -1;
	logctx *lc;

	if((lc = get_thread_logctx()) == NULL){
		goto done;
	}
	if(!crash_log){
		goto done;
	}
	if(avail_crash_log){
		timenag("Closing unused crash log\n");
		avail_crash_log = NULL;
		if(fclose(crash_log) == EOF){
			moan("Couldn't close crash log at %p\n",crash_log);
		}
		Unlink(crash_fn);
		goto done;
	}
	if(lc->lfile != crash_log){
		bitch("Crash log %p claimed, cur = %p\n",crash_log,lc->lfile);
		goto done;
	}
	if((fd = fileno(crash_log)) < 0){
		moan("Couldn't extract fd from crash log %p\n",crash_log);
		goto done;
	}
	nag("Truncating crash log at fd %d\n",fd);
	timenag("Halting! Exit code: %d\n",exit_status);
	Ftruncate(fd,lc->lfile_offset);
	// closing crash_log here in fatal sig handler context -> lockup, thus:
	lc->lfile = NULL; // FIXME

done:
	if(fd < 0){
		timenag("Halting with exit code %d\n",exit_status);
	}
	crash_log = NULL;
	free_logctx(lc);
	free(logdir);
	logdir = NULL;
	return;
}

void truncate_crash_log(int exit_status){
	if(pthread_mutex_trylock(&logdir_lock)){
		return;
	}
	truncate_crash_log_locked(exit_status);
	pthread_mutex_unlock(&logdir_lock);
}
