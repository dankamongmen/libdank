#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/procfs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/pidlock.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/lexers.h>
#include <libdank/objects/logctx.h>

static int
pid_exists(pid_t pid){
	if(kill(pid,0)){
		if(errno == ESRCH){
			return 0;
		}
	}
	return 1;
}

static int
read_pidlock_pid(const char *lockfn){
	char pidbuf[64];
	const char *end;
	struct stat st;
	pid_t pid = -1;
	int fd,r;

	if((fd = Open(lockfn,O_RDONLY)) < 0){
		goto done;
	}
	if(Fstat(fd,&st)){
		goto done;
	}
	if(S_ISDIR(st.st_mode)){
		bitch("%s is a directory\n",lockfn);
		goto done;
	}
	if((r = Read(fd,pidbuf,sizeof(pidbuf) - 1)) < 0){
		goto done;
	}
	end = pidbuf;
	pidbuf[r] = '\0';
	if(lex_s32(&end,&pid) < 0){ // assumes 32-bit pids! FIXME
		goto done;
	}
	// Linux has kernel.pid_max sysctl; FreeBSD has PID_MAX in sys/proc.h

done:
	if(fd >= 0 && Close(fd)){
		return -1;
	}
	return pid;
}

int is_pidlock_ours(const char *pidlock,const char *cmd,pid_t *pid){
	char *procname;
	pid_t nullbase;

	if(pid == NULL){
		pid = &nullbase;
	}
	if((*pid = read_pidlock_pid(pidlock)) < 0){
		return 0;
	}
	if(pid_exists(*pid) == 0){
		return 0;
	}
	nag("PID %d (%s) exists\n",*pid,pidlock);
	if(cmd == NULL){ // FIXME we can't do any better without a cmd...
		return 1;
	}
	if((procname = procfs_cmdline_argv0(*pid)) == NULL){
		return 0;
	}
	if(strcmp(procname,cmd)){
		nag("/proc/%d reports '%s'; we wanted '%s'\n",*pid,procname,cmd);
		Free(procname);
		return 0;
	}
	nag("/proc/%d reports '%s'\n",*pid,procname);
	Free(procname);
	return 1;
}

// There was already a PID lock, but it could be from a long-dead messy
// process. Check the PID; if it exists, error out (don't remove the lock!).
// Otherwise, remove the logfile and create a new one.
static int
handle_lockfile(const char *lockfn,const char *cmdname){
	mode_t mode = S_IWRITE | S_IREAD | S_IRGRP | S_IROTH;

	nag("Found existing PIDlock %s\n",lockfn);
	if(is_pidlock_ours(lockfn,cmdname,NULL)){
		moan("%s describes a valid process\n",lockfn);
		return -1;
	}
	if(purge_lockfile(lockfn) == 0){
		nag("Removed stale PIDlock %s\n",lockfn);
	}
	return OpenCreat(lockfn,O_EXCL | O_WRONLY | O_TRUNC | O_CREAT,mode);
}

static int
check_lockfile(const char *lockfn,const char *cmdname){
	mode_t mode;
	int fd;

	mode = S_IWRITE | S_IREAD;
	fd = open(lockfn,O_CREAT | O_EXCL | O_WRONLY,mode);
	if(fd < 0){
		// EEXIST:  file existed (tested with O_EXCL | O_CREAT)
		if(errno == EEXIST){
			if((fd = handle_lockfile(lockfn,cmdname)) < 0){
				return -1;
			}
		}else{
			fprintf(stderr,"Error opening PIDlock %s (%s)\n",
					lockfn,strerror(errno));
			return -1;
		}
	}
	return fd;
}

static int
write_lockfile(int fd){
	int ret = 0,errcpy;
	/* 64-bit INT_MAX is 21 chars, linux 2.5 is currently
	 * considering moving from 16 to 32-bit pids */
	char pidbuf[24];
	pid_t pid;

	pid = getpid();
	if(snprintf(pidbuf,sizeof(pidbuf),"%d\n",pid) >= (ssize_t)sizeof(pidbuf)){
		ret = -1;
	}else if(write(fd,pidbuf,strlen(pidbuf)) < (ssize_t)strlen(pidbuf)){
		ret = -1;
	}

	errcpy = errno;
	if(close(fd) != 0){
		ret = -1;
	}else{
		errno = errcpy;
	}
	return ret;
}

// Check the (existing) pidfile and determine whether it a) refers to an
// existing process, and optionally that this process b) is actually an
// instance of ourselves, not some random other process. This latter check
// requires that a procfs filesystem supporting a "cmdline" file be mounted at
// /proc, and that the program be invoked via the same literal reference across
// instances (parameters may differ). To disable the check, simply provide NULL
// as cmd. The pid referenced will be stored into *pid if it is non-NULL.
int open_exclusive_pidlock(const char *lockfn,const char *cmdname){
	int fd;

	if((fd = check_lockfile(lockfn,cmdname)) < 0){
		// Either another valid instance is running, or we couldn't
		// write the lockfile. Either way, don't remove it.
		return -1;
	}
	if(write_lockfile(fd)){	// closes the lockfile, always
		if(errno){
			fprintf(stderr,"Error writing to %s (%s)\n",
					lockfn,strerror(errno));
		}else{
			fprintf(stderr,"Error writing PIDlock %s\n",lockfn);
		}
		if(unlink(lockfn) != 0){
			fprintf(stderr,"Warning: error removing %s (%s)\n",
					lockfn,strerror(errno));
		}
		return -1;
	}
	return 0;
}

int purge_lockfile(const char *lockfn){
	return Unlink(lockfn);
}
