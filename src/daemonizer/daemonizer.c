#include <getopt.h>
#include <stdlib.h>
#include <libdank/utils/privs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/pidlock.h>
#include <libdank/apps/initpublic.h>

#define APPNAME "daemonizer"

static void
usage(const char *name,int status){
	FILE *out = status == EXIT_SUCCESS ? stdout : stderr;

	fprintf(out,"\n");
	fprintf(out,"usage: %s [args] -- executable [execargs]\n",name);
	fprintf(out,"\n");
	fprintf(out,"ACTIONS (only one may be supplied)\n");
	fprintf(out," --start: run the executable (default action)\n");
	fprintf(out," --stop: stop the daemon, remove pidlock (requires -p)\n");
	fprintf(out," --status: check for daemon (requires -p)\n");
	fprintf(out,"\n");
	fprintf(out,"ARGUMENTS\n");
	fprintf(out," -h: this message\n");
	fprintf(out," -c: allow corefiles (default is to disable generation)\n");
	fprintf(out," -d: daemonize (default is to run in foreground)\n");
	fprintf(out," -u user: run as user\n");
	fprintf(out," -g group[,supp...]: run as group(s)\n");
	fprintf(out," -p pidlock: write pid to pidlock\n");
	fprintf(out," -r runcount: respawn up to runcount times, 0 for unbounded\n");
	fprintf(out," -L delaysec: wait delaysec between respawns (requires -r)\n");
	fprintf(out," -R path: change the root to path\n");
	fprintf(out," -T boundsec: only allow instances to run for boundsec\n");
	exit(status);
}

#define SET_FLAG_ONCE(opt,var,value) \
case opt: \
	if(var){ \
		bitch("Used option twice: %c\n",opt); \
		usage(argv[0],EXIT_FAILURE); \
	} \
	(var) = (value); \
	break

#define SET_OPT_ONCE(opt,var) SET_FLAG_ONCE(opt,var,optarg)

#define ULOPT_UNDEFINED(val) ((val) == ULONG_MAX)

#define SET_ULOPT_ONCE(opt,var) \
case opt: { \
	char *m_endptr; \
	if(!ULOPT_UNDEFINED(var)){ \
		bitch("Set parameter twice: %c\n",opt); \
		usage(argv[0],EXIT_FAILURE); \
	} \
	if(((var) = strtoul(optarg,&m_endptr,0)) == ULONG_MAX || m_endptr == optarg){ \
		bitch("Invalid value for %c: %s\n",opt,optarg); \
		usage(argv[0],EXIT_FAILURE); \
	} \
	} break

static int
start_command(char **argv,const char *pidlock,unsigned long respawn,
			unsigned long delaysec,unsigned long boundsec,
			int daemonize){
	unsigned long i;

	if(pidlock && pidlock[0]){
		if(access(pidlock,F_OK) == 0 || errno != ENOENT){
			if(is_pidlock_ours(pidlock,argv[optind],NULL) ||
				is_pidlock_ours(pidlock,argv[0],NULL)){
				return -1;
			}else if(purge_lockfile(pidlock)){
				nag("Warning: couldn't remove pidlock at %s\n",pidlock);
			}
		}
	}
	if(daemonize && Daemon(argv[0],0,0)){
		return -1;
	}
	if(pidlock && pidlock[0]){
		if(open_exclusive_pidlock(pidlock,argv[optind])){
			return -1;
		}
	}
	if(!ULOPT_UNDEFINED(boundsec)){ // FIXME
		bitch("Haven't yet implemented -T\n");
		return -1;
	}
	if(ULOPT_UNDEFINED(respawn)){
		// FIXME this results in the pidlock not being removed. perhaps
		// if we overloaded _atexit() via LD_LIBRARY_PATH mangling...?
		if(Execvp(argv[optind],argv + optind)){
			return -1;
		}
	}
	for(i = 0 ; i < respawn || !respawn ; ++i){
		pid_t pid;

		if((pid = Fork()) == 0){
			if(Execvp(argv[optind],argv + optind)){
				return -1;
			}
		}else if(pid > 0){
			int status;

			if(Waitpid(pid,&status,0) < 0){
				return -1;
			}
		}else{
			return -1;
		}
		if(!ULOPT_UNDEFINED(delaysec) && Sleep(delaysec)){
			return -1;
		}
	}
	return 0;
}

static int
status_command(char **argv,const char *pidlock,unsigned long respawn,
			unsigned long delaysec,unsigned long boundsec,
			int daemonize){
	pid_t pid;

	if(!pidlock || !pidlock[0]){
		bitch("Can't use --status without a pidlock\n");
		usage(argv[0],EXIT_FAILURE);
	}
	if(!ULOPT_UNDEFINED(respawn) || !ULOPT_UNDEFINED(delaysec) ||
			!ULOPT_UNDEFINED(boundsec) || daemonize){
		bitch("Can't use --status with -L, -T, -r or -d options\n");
		usage(argv[0],EXIT_FAILURE);
	}
	if(!is_pidlock_ours(pidlock,argv[optind],&pid) &&
		!is_pidlock_ours(pidlock,argv[0],&pid)){
		// FIXME would be nice to remove pidfile if the process no
		// longer exists, as opposed to having exited prior
		bitch("Couldn't find process using %s\n",pidlock);
		return -1;
	}
	nag("Found process at PID %d\n",pid);
	return 0;
}

static int
stop_command(char **argv,const char *pidlock,unsigned long respawn,
			unsigned long delaysec,unsigned long boundsec,
			int daemonize){
	pid_t pid;

	if(!pidlock || !pidlock[0]){
		bitch("Can't use --stop without a pidlock\n");
		usage(argv[0],EXIT_FAILURE);
	}
	if(!ULOPT_UNDEFINED(respawn) || !ULOPT_UNDEFINED(delaysec) ||
			!ULOPT_UNDEFINED(boundsec) || daemonize){
		bitch("Can't use --stop with -L, -T, -r or -d options\n");
		usage(argv[0],EXIT_FAILURE);
	}
	if(!is_pidlock_ours(pidlock,argv[optind],&pid) &&
		!is_pidlock_ours(pidlock,argv[0],&pid)){
		// FIXME would be nice to remove pidfile if the process no
		// longer exists, as opposed to having exited prior
		bitch("Couldn't find process using %s\n",pidlock);
		return -1;
	}
	if(Kill(-pid,SIGTERM)){
		return -1;
	}
	return 0;
}

// If we chdir("/") due to daemonization, relative paths will no longer refer
// to the files they (almost certainly) meant to. Thus, relative pathnames in
// command line arguments, where semantically applicable, are rewritten in
// terms of the original working directory (this still doesn't solve the
// problem for chroots).
static int
prepare_file_argument(char *buf,size_t len,const char *arg,const char *cwd){
	int snret;

	if(buf == NULL || len == 0){
		return  -1;
	}
	if(arg == NULL){
		buf[0] = '\0';
		return 0;
	}
	if(arg[0] == '/'){
		snret = snprintf(buf,len,"%s",arg);
	}else{
		snret = snprintf(buf,len,"%s/%s",cwd,arg);
	}
	if(snret < 0 || (size_t)snret >= len){
		bitch("Couldn't normalize filename viz %s: %s\n",cwd,arg);
		return -1;
	}
	return 0;
}

static int
preclude_corefiles(void){
	struct rlimit rlim = { .rlim_cur = 0, .rlim_max = 0, };

	return Setrlimit(RLIMIT_CORE,&rlim);
}

int main(int argc,char **argv){
	enum {
		LONGOPT_START,
		LONGOPT_STOP,
		LONGOPT_STATUS,
	} command = LONGOPT_START;
	int longoptvar = 0;
	const struct option long_options[] = {
		{
			.name = "stop",
			.has_arg = 0,
			.flag = &longoptvar,
			.val = LONGOPT_STOP,
		},{
			.name = "start",
			.has_arg = 0,
			.flag = &longoptvar,
			.val = LONGOPT_START,
		},{
			.name = "status",
			.has_arg = 0,
			.flag = &longoptvar,
			.val = LONGOPT_STATUS,
		},{ .name = NULL, .has_arg = 0, .flag = NULL, .val = 0, }
	};
	unsigned long respawn = ULONG_MAX,delaysec = ULONG_MAX,boundsec = ULONG_MAX;
	const char *user = NULL,*groups = NULL,*pidlock_arg = NULL,*newroot = NULL;
	char cwd[PATH_MAX],pidlock[PATH_MAX];
	int gopt,daemonize = 0,corefiles = 0;

	if(libdank_vercheck()){
		return EXIT_FAILURE;
	}
	if(!Getcwd(cwd,sizeof(cwd))){
		return EXIT_FAILURE;
	}
	opterr = 0;
	while((gopt = getopt_long(argc,argv,":u:g:r:L:R:T:hcdp:",long_options,NULL)) >= 0){
		switch(gopt){
		case 'h':
			usage(argv[0],EXIT_SUCCESS);
			break;
		SET_OPT_ONCE('u',user);
		SET_OPT_ONCE('g',groups);
		SET_OPT_ONCE('p',pidlock_arg);
		SET_OPT_ONCE('R',newroot);
		SET_ULOPT_ONCE('r',respawn);
		SET_ULOPT_ONCE('L',delaysec);
		SET_ULOPT_ONCE('T',boundsec);
		SET_FLAG_ONCE('c',corefiles,1);
		SET_FLAG_ONCE('d',daemonize,1);
		case 0:
			switch(longoptvar){
			case LONGOPT_START:
			case LONGOPT_STOP:
			case LONGOPT_STATUS:
				if(command != LONGOPT_START){
					bitch("Supplied multiple commands!\n");
					usage(argv[0],EXIT_FAILURE);
				}
				command = longoptvar;
				break;
			default:
				bitch("Unknown longoptvar: %d\n",longoptvar);
				usage(argv[0],EXIT_FAILURE);
			}
			break;
		case ':':
			bitch("Parameter requires argument: %c\n",optopt);
			usage(argv[0],EXIT_FAILURE);
			break;
		case '?':
			bitch("Unknown parameter: %c\n",optopt);
		default:
			usage(argv[0],EXIT_FAILURE);
		}
	}
	if(drop_privs(user,groups)){
		usage(argv[0],EXIT_FAILURE);
	}
	if(!corefiles && preclude_corefiles()){
		return EXIT_FAILURE;
	}
	if(prepare_file_argument(pidlock,sizeof(pidlock),pidlock_arg,cwd)){
		usage(argv[0],EXIT_FAILURE);
	}
	if(newroot && Chroot(newroot)){
		usage(argv[0],EXIT_FAILURE);
	}
	if(!ULOPT_UNDEFINED(delaysec) && ULOPT_UNDEFINED(respawn)){
		bitch("-L argument used without -r\n");
		usage(argv[0],EXIT_FAILURE);
	}
	if(optind >= argc){
		bitch("Need a command to work with!\n");
		usage(argv[0],EXIT_FAILURE);
	}
	switch(command){
		case LONGOPT_STOP:
			if(stop_command(argv,pidlock,respawn,delaysec,boundsec,daemonize)){
				return EXIT_FAILURE;
			}
			break;
		case LONGOPT_START:
			if(start_command(argv,pidlock,respawn,delaysec,boundsec,daemonize)){
				return EXIT_FAILURE;
			}
			break;
		case LONGOPT_STATUS:
			if(status_command(argv,pidlock,respawn,delaysec,boundsec,daemonize)){
				return EXIT_FAILURE;
			}
			break;
		default:
			bitch("Internal error (command %d)\n",command);
			return EXIT_FAILURE;
	}
	// FIXME we should register this with atexit(3) so that it's cleaned up
	// when we exit due to signals, etc...
	if(pidlock_arg && purge_lockfile(pidlock)){
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
