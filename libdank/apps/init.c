#include <locale.h>
#include <stdlib.h>
#include <langinfo.h>
#include <sys/stat.h>
#include <libdank/version.h>
#include <libdank/arch/cpu.h>
#include <libdank/apps/init.h>
#include <libdank/utils/privs.h>
#include <libdank/apps/environ.h>
#include <libdank/arch/cpucount.h>
#include <libdank/utils/confstr.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/pidlock.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/tracing/oops.h>
#include <libdank/modules/fileconf/sbox.h>
#include <libdank/modules/fileconf/sbox.h>
#include <libdank/modules/ctlserver/ctlserver.h>

static const int fatal_signals[] = {
	SIGSEGV,
	SIGFPE,
	SIGBUS,
	SIGILL,
	SIGABRT,
};

static const int ignored_signals[] = {
	SIGPIPE,
	SIGHUP,
};

static const int cancellation_signals[] = {
	SIGTERM,
	SIGINT,
	SIGQUIT,
};

const app_def *application = NULL;

// posix says when we raise a synchronous hardware signal
// (SIGFPE, SIGSEGV, etc...), the thread which raised it provides the context
// for the (process-wide) sigaction.
static void
fatal_sig_handler(int signum,siginfo_t *si,void *uctx){
	logctx lc;

	log_crash(&lc);
	log_oops(signum,si,uctx);
	stop_logging(signum + 128);
	raise(signum); // see handle_fatal_sigs() -- we use SA_RESETHAND
}

static int
handle_sigs(const int *sigs,unsigned n,const struct sigaction *sa){
	unsigned z;

	for(z = 0 ; z < n ; ++z){
		if(Sigaction(sigs[z],sa,NULL)){
			return -1;
		}
		nag("Set signal handler for %s\n",strsignal(sigs[z]));
	}
	return 0;
}

int handle_fatal_sigs(void){
	struct sigaction sigs;
	unsigned signum;

	signum = sizeof(fatal_signals) / sizeof(*fatal_signals);
	memset(&sigs,0,sizeof(sigs));
	sigemptyset(&sigs.sa_mask);
	// (OBSOLETE) SA_ONSTACK could be used, after setting up an alternate
	// stack via sigaltstack(), to ensure signal delivery after stack
	// overflow. We currently determine signal source via examining the
	// stack of each thread, however, and besides sigaltstack() and glibc
	// 2.2.5 don't play together nicely (experiments show cyclesoaking
	// loop):
	//  http://sources.redhat.com/ml/libc-alpha/2003-02/msg00071.html
	//  http://sources.redhat.com/ml/libc-alpha/2003-01/msg00244.html
	//  http://sources.redhat.com/ml/libc-hacker/2003-12/msg00076.html
	// SA_RESETHAND, meanwhile, could be used to allow the fatal sig
	// handler to raise the received signal, rather than SIGKILL, exporting
	// more information to the parent process. Unfortunately, this seems to
	//  - cause a cyclesoaking loop if neither RESETHAND | NODEFER are used
	//  - leave a zombie thread around (main thread) if either/both are
	// The zombie thread is the main thread, presumably blocked in sigwait.
	sigs.sa_flags |= SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
	sigs.sa_sigaction = fatal_sig_handler;
	return handle_sigs(fatal_signals,signum,&sigs);
}

int handle_ignored_sigs(void){
	struct sigaction sigs;
	unsigned signum;

	signum = sizeof(ignored_signals) / sizeof(*ignored_signals);
	memset(&sigs,0,sizeof(sigs));
	sigemptyset(&sigs.sa_mask);
	sigs.sa_handler = SIG_IGN;
	return handle_sigs(ignored_signals,signum,&sigs);
}

static int
handle_cancellation_sigs(sigset_t *set){
	struct sigaction sigs;
	unsigned z,count;

	count = sizeof(cancellation_signals) / sizeof(*cancellation_signals);
	sigemptyset(set);
	memset(&sigs,0,sizeof(sigs));
	sigemptyset(&sigs.sa_mask);
	// sigwait() is used to block on cancellation messages. These are
	// required to be blocked, and not set to SIG_IGN, so set to SIG_DFL.
	// (what if the default action, however, is to ignore, ala SIGCHLD?)
	sigs.sa_handler = SIG_DFL;
	if(handle_sigs(cancellation_signals,count,&sigs) < 0){
		return -1;
	}
	for(z = 0 ; z < count ; ++z){
		if(Sigaddset(set,cancellation_signals[z]) < 0){
			return -1;
		}
	}
	return 0;
}

static int
init_signals(sigset_t *set,const sigset_t *blockset){
	if(handle_fatal_sigs()){
		return -1;
	}
	// all ignored sigs, predictably, get SIG_IGN
	if(handle_ignored_sigs()){
		return -1;
	}
	if(handle_cancellation_sigs(set)){
		return -1;
	}
	if(pthread_sigmask(SIG_SETMASK,set,NULL)){
		moan("Couldn't set thread signal mask\n");
		return -1;
	}
	if(pthread_sigmask(SIG_BLOCK,blockset,NULL)){
		moan("Couldn't augment thread signal mask\n");
		return -1;
	}
	return 0;
}

static const char MEMLIMIT_ENVSTR[] = "MEMLIMIT";

static const char *std_keys[] = {
	MEMLIMIT_ENVSTR,
	NULL
};

static char *std_values[] = {
	NULL,
	NULL
};

static envset std_envset = {
	.keys = std_keys,
	.values = std_values,
};

static int
check_memory_limit(const envset *e){
	const char * const *key = e->keys;
	char **value = e->values;
	size_t memlimit = 0;

	while(*key){
		if(strcmp(*key,MEMLIMIT_ENVSTR) == 0){
			if(*value == NULL){
				nag("No memory limit specified (envvar %s)\n",MEMLIMIT_ENVSTR);
				break;
			}
			if(sscanf(*value,"%zu",&memlimit) != 1){
				bitch("Expected envvar %s=uint, got %s\n",*key,*value);
				break;
			}
			break;
		}
		++key;
		++value;
	}
	if(*key == NULL){
		bitch("Memlimit (%s) wasn't in envset\n",MEMLIMIT_ENVSTR);
		return -1;
	}
	return limit_memory(memlimit);
}

static void
predaemonize_fd_genocide(void){
	nag("Flushing stdout\n");
	Fflush(stdout);
	Fflush(stderr);
}

static int
check_app_ctx(const app_ctx *ctx){
	if(ctx == NULL){
		bitch("Initialized with NULL app_ctx\n");
		return -1;
	}
	if(ctx->lockfile){
		bitch("Initialized with non-NULL app_ctx->lockfile\n");
		return -1;
	}
	if(ctx->ctlsrvsocket){
		bitch("Initialized with non-NULL app_def->ctlsrvsocket\n");
		return -1;
	}
	return 0;
}

static int
check_app_def(const app_def *def){
	if(def == NULL){
		bitch("Initialized with NULL app_def\n");
		return -1;
	}
	if(def->appname == NULL){
		bitch("Initialized with NULL app_def->appname\n");
		return -1;
	}
	return 0;
}

static void
usage(const char *name,const char *def_lockfile,const char *def_ctlsock){
	nag("usage: %s [ -d | -v ] [ -u uname ] [ -g gname ] [ -c ctlsock ] [ -p pidfile ] [ -l logdir ] [ -o confdir ] [ -R newroot ]\n",name);
	nag("\t-d: daemonize\n");
	nag("\t-v: dump output to stdio\n");
	nag("\t-p: write PID to pidfile\n");
	nag("\t-l: write logs to logdir\n");
	nag("\t-c: use socket at ctlsock\n");
	nag("\t-o: keep configuration files in confdir\n");
	nag("\t-u: change to the UID associated with uname\n");
	nag("\t-g: change to the GID associated with gname\n");
	nag("\t-R: change root to path specified by newroot\n");
	nag("default pidfile: %s\n",def_lockfile ? def_lockfile : "none");
	nag("default ctlsock: %s\n",def_ctlsock ? def_ctlsock : "none");
}

static void
describe_system(void){
	nag("compiled %s\n",Libdank_Build_Date);
	nag("compiled with %s\n",Libdank_Compiler);
#ifdef _CS_GNU_LIBC_VERSION
	{
		char *cstr;

		if( (cstr = confstr_dyn(_CS_GNU_LIBC_VERSION)) ){
			char *pcstr;

			pcstr = confstr_dyn(_CS_GNU_LIBPTHREAD_VERSION);
			nag("compiled against %s (%s)\n",cstr,
				pcstr ? pcstr : "couldn't detect thread system");
			Free(pcstr);
			Free(cstr);
		}else{
			nag("not using glibc > 2.3.2\n");
		}
	}
#else
	nag("not using a glibc with confstr(3)\n");
#endif
}

int libdank_vercheck_internal(const char *ver){
	if(strcmp(ver,LIBDANK_REVISION)){
		bitch("Compiled against libdank-r%s, linked against -r%s\n",
				ver,LIBDANK_REVISION);
		return -1;
	}
	return 0;
}

static int
use_environment_locale(void){
	const char *codeset,*locale;

	if((locale = Setlocale(LC_ALL,"")) == NULL){
		return -1;
	}
	nag("Locale is: %s\n",locale);
	if((codeset = nl_langinfo(CODESET)) == NULL){
		bitch("Couldn't get local encoding\n");
		return -1;
	}
	nag("Encoding is: %s\n",codeset);
	return 0;
}

// persevere on, and just return error as a series of |= -1's.  let the
// calling app decide how to handle failure, except for pidlock issues. FIXME??
int app_init_vercheck(const char *ver,logctx *lc,const app_def *app,
			app_ctx *ctx,int argc,char **argv,
			const char *def_lockfile,const char *def_ctlsock){
	const char *logdir,*confdir,*lockfile = def_lockfile,*ctlsock = def_ctlsock;
	const char *newuser = NULL,*newgroup = NULL,*newroot = NULL;
	int daemonize = 0,ret = 0,stdio = 0,opt;

	if(libdank_vercheck_internal(ver) || check_app_ctx(ctx) || check_app_def(app)){
		return -1;
	}
	if(use_environment_locale()){
		return -1;
	}
	confdir = app->confdir;
	logdir = app->logdir;
	ctx->lockfile = NULL;
	opterr = 0;
	while((opt = getopt(argc,argv,":dvp:l:c:o:u:g:R:")) >= 0){
		if(opt == 'd'){
			if(stdio){
				bitch("-d cannot be used with -v\n");
				usage(argv[0],def_lockfile,def_ctlsock);
				return -1;
			}
			daemonize = 1;
		}else if(opt == 'v'){
			if(daemonize){
				bitch("-v cannot be used with -d\n");
				usage(argv[0],def_lockfile,def_ctlsock);
				return -1;
			}
			stdio = 1;
		}else if(opt == 'p'){
			lockfile = optarg;
		}else if(opt == 'o'){
			confdir = optarg;
		}else if(opt == 'l'){
			logdir = optarg;
		}else if(opt == 'c'){
			ctlsock = optarg;
		}else if(opt == 'u'){
			newuser = optarg;
		}else if(opt == 'R'){
			newroot = optarg;
		}else if(opt == 'g'){
			newgroup = optarg;
		}else if(opt == ':'){
			bitch("Option requires argument: -%c\n",optopt);
			usage(argv[0],def_lockfile,def_ctlsock);
			return -1;
		}else{
			bitch("Illegal option: -%c\n",optopt);
			usage(argv[0],def_lockfile,def_ctlsock);
			return -1;
		}
	}
	umask(S_IRWXO | S_IRWXG);
	ret |= inspect_env(&std_envset,app->environ);
	// Only root (or users with CAP_SYS_CHROOT) can call chroot(2), so go
	// ahead and do that before dropping privileges...
	if(newroot && Chroot(newroot)){
		ret = -1;
	}
	if(drop_privs(newuser,newgroup)){
		return -1;
	}
	if(daemonize){ // We'll have null stdout/stderr/stdin past this point!
		predaemonize_fd_genocide();
		if(Daemon(app->appname,0,0)){
			ret = -1;
		}
	}
	if(lockfile){
		char *lfile;

		if((lfile = strdup(lockfile)) == NULL){
			fprintf(stderr,"Allocation failure for pidlock %s\n",lockfile);
			return -1;
		}
		if(open_exclusive_pidlock(lockfile,NULL)){
			free(lfile);
			return -1;
		}
		ctx->lockfile = lfile;
	}else{
		nag("Not writing a PID lock\n");
	}
	if(ret){
		bitch("Warning: error prior to log startup (%d)\n",ret);
	}
	if(lc){
		ret |= init_logging(lc,logdir,stdio);
	}
	timenag("%s (using libdank %s rev %s) %s\n",app->appname,
			Libdank_Version,LIBDANK_REVISION,
			app->obnoxiousness ? app->obnoxiousness : "");
	describe_system();
	ret |= id_cpu();
	ret |= init_fileconf(confdir); // sets up libxml2
	ret |= detect_num_processors() ? 0 : -1;
	ret |= check_memory_limit(&std_envset);
	ret |= init_signals(&ctx->waitsigs,&app->blocksigs);
	if(ctlsock){
		if(init_ctlserver(ctlsock) == 0){
			ret |= init_log_server();
			ctx->ctlsrvsocket = ctlsock;
		}else{
			ret = -1;
		}
	}else{
		nag("Not launching ctlserver\n");
	}
	timenag("appinit returning %d\n",ret);
	application = app;
	return ret;
}
