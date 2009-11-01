#include <math.h>
#include <dlfcn.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <cunit/cunit.h>
#include <libdank/arch/cpu.h>
#include <libdank/utils/time.h>
#include <libdank/utils/dlsym.h>
#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/ersatz/compat.h>
#include <libdank/objects/lexers.h>
#include <libdank/utils/memlimit.h>
#include <libdank/arch/profiling.h>
#include <libdank/apps/initpublic.h>
#include <libdank/modules/ui/color.h>
#include <libdank/modules/ui/ncurses.h>
#include <libdank/modules/logging/logdir.h>
#include <libdank/modules/logging/logging.h>

// Use -r to preclude running a failed test verbosely (when not in verbose
// mode); no matter the result of this failed test, the previous (failed)
// result will be used.
static int rerun_failed_test_verbosely = 1;

// PID of the child trampoline, and thus PGID of the currently running test and
// all its children (assuming none of them have called setpgid() themselves, or
// otherwise left the process group). FIXME we need rename this
static pid_t pid;

static char cwd[PATH_MAX];
static const char *config_dir;
static unsigned benchmark_tests;
static int verbose,run_all_tests,failloc_tests;

static const declared_test *BUILTIN_TEST_SUITES[] = {
	CUNIT_TESTS,
	ARCH_TESTS,
	OFFT_TESTS,
	MAGIC_TESTS,
	FDS_TESTS,
	EVENT_TESTS,
	VM_TESTS,
	MMAP_TESTS,
	PTHREADS_TESTS,
	USTRING_TESTS,
	STRING_TESTS,
	LOGCTX_TESTS,
	TIMEVAL_TESTS,
	XML_TESTS,
	FILECONF_TESTS,
	PROCFS_TESTS,
	CRLFREADER_TESTS,
	CTLSERVER_TESTS,
	HEALTH_TESTS,
	DNS_TESTS,
	RFC2396_TESTS,
	RFC3330_TESTS,
	NETLINK_TESTS,
	HEX_TESTS,
	DLSYM_TESTS,
	SSL_TESTS,
	SLALLOC_TESTS,
	LRUPAT_TESTS,
	INTERVAL_TREE_TESTS,
	NULL
};

static unsigned dynamic_test_suite_count;
static const declared_test **dynamic_test_suites;

// Always returns a valid pointer, and is thus a) safe to print without
// checking the return, and b) useless unless you've seen a function fail.
static const char *
Dlerror(void){
	const char *r;

	if((r = dlerror()) == NULL){
		r = "No linker error";
	}
	return r;
}

static int
add_extension_testset(const char *setname,const declared_test *testset){
	typeof(*dynamic_test_suites) *tmp;

	if((tmp = Realloc("testsuite",dynamic_test_suites,
			sizeof(*tmp) * (dynamic_test_suite_count + 1))) == NULL){
		return -1;
	}
	tmp[dynamic_test_suite_count++] = testset;
	dynamic_test_suites = tmp;
	printf("Adding %s series: ",setname);
	while(testset->name){
		printf("[%s] ",testset->name);
		++testset;
	}
	printf("\n");
	return 0;
}

static int
add_tests_from_list(void *obj,const char **list){
	const char *testsetname;

	while( (testsetname = *list++) ){
		const declared_test *testset;
		const char *errstr;

		if((testset = Dlsym(obj,testsetname,&errstr)) == NULL){
			fprintf(stderr," Couldn't get a valid testset from %s\n",testsetname);
			return -1;
		}
		if(add_extension_testset(testsetname,testset)){
			return -1;
		}
	}
	return 0;
}

// FIXME close the modules at the end
static int
add_tests_from_so(const char *so){
	const char *dlerr = NULL,*waserr;
	const char **tests;
	void *obj;
	int ret = -1;

	dlerror();
	if((obj = dlopen(so,RTLD_NOW | RTLD_GLOBAL)) == NULL){
		dlerr = Dlerror();
		fprintf(stderr,"Couldn't open object at %s (%s?)\n",so,dlerr);
		return -1;
	}
	if((tests = Dlsym(obj,"CUNIT_EXTENSIONS",&waserr)) == NULL){
		fprintf(stderr," Couldn't get a valid extension from %s\n",so);
		goto done;
	}
	if(add_tests_from_list(obj,tests)){
		goto done;
	}
	printf("Added tests from %s.\n",so);
	ret = 0;

done:
	// Don't use Dlerror(); it'll return a valid pointer in all cases
	if( (dlerr = dlerror()) ){
		fprintf(stderr,"Error linking %s: %s\n",so,dlerr);
		return -1;
	}
	return ret;
}

// Solving the halting problem it ain't, but this should work well outside of
// possible I/O hangs. It also keeps our testing sane. These defaults can be
// overridden with mb_required and sec_required in test definitions.
static const unsigned TEST_RUNTIME_MAX = 10;
static const unsigned TEST_MEGABYTE_MAX = 64;

static unsigned
run_test_context(const testptr fxn){
	intmax_t allocs;
	unsigned ret;
	logctx *lc;

	if((lc = get_thread_logctx()) == NULL){
		return EXIT_TESTFAILED;
	}
	allocs = outstanding_allocs();
	if((ret = fxn()) == 0){
		ret = EXIT_HACKTESTSUCCESS;
	}else{
		ret = EXIT_TESTFAILED;
	}
	fflush(stdout);
	fflush(stderr);
	reset_ustring(lc->out);
	if((allocs != outstanding_allocs())){
		ret = EXIT_MEMLEAK | (ret & EXIT_TESTFAILED);
		printf(" Unbalanced allocs, %ju != %ju.\n",
			allocs,outstanding_allocs());
	}
	// if we were verbose, errors were printed inline
	if((ret & EXIT_TESTFAILED) && lc->err->current && !verbose){
		fprintf(stderr," %s",lc->err->string);
	}
	reset_ustring(lc->err);
	if(verbose){
		stringize_memory_usage(lc->out);
		printf(" %s\n",lc->out->string);
	}
	reset_logctx_ustrings();
	return ret;
}

static unsigned
trampoline_test(const declared_test *dt){
	const testptr fxn = dt->testfxn;

	if((pid = Fork()) < 0){
		return EXIT_TESTFAILED;
	}else if(pid == 0){ // child!
		intmax_t failloc = 0,benchmarks = 0,usec = 0,usecsquared = 0;
		struct sigaction sa;
		unsigned sec,mb;
		int ret;

		memset(&sa,0,sizeof(sa));
		if(sigaction(SIGINT,&sa,NULL)){
			fprintf(stderr,"Couldn't reset signal handler (%s?)!\n",strerror(errno));
			_exit(EXIT_INTERNALEXIT);
		}
		if(setpgid(0,0)){ // start a new process group equal to our PID
			fprintf(stderr,"Couldn't set process group (%s?)!\n",strerror(errno));
			_exit(EXIT_INTERNALEXIT);
		}
		mb = dt->mb_required ? dt->mb_required : TEST_MEGABYTE_MAX;
		sec = dt->sec_required ? dt->sec_required : TEST_RUNTIME_MAX;
		if(verbose == 0){
			printf("aborting in %us.\n",sec);
		}else if(verbose && !rerun_failed_test_verbosely){
			printf("Warning: verbose mode! No timeout!\n");
		}else{
			printf("\n");
		}
		if(limit_memory(mb * 1024 * 1024)){
			_exit(EXIT_NEEDMORERAM);
		}
		do{
			struct timeval t0,t1;

			if(failloc){
				// FIXME need to reset directory in case of
				// chdir that wasn't cleaned up
				printf("Alloc fail test %jd\n",failloc - 1);
				failloc_on_n(failloc - 1);
			}
			if(!verbose || rerun_failed_test_verbosely){
				alarm(sec);
			}
			Gettimeofday(&t0,NULL);
			ret = run_test_context(fxn);
			Gettimeofday(&t1,NULL);
			if(failloc){ // we're in the -f testing loop
				if(ret == EXIT_TESTFAILED){
					++failloc;
				}else{
					failloc = 0; // stop, got non-fail result
				}
			}else if(benchmarks){
				intmax_t us;

				if(ret != EXIT_HACKTESTSUCCESS){
					fprintf(stderr," %d\n",ret);
					fprintf(stderr," Error during benchmarking; aborting run\n");
					exit(ret);
				}
				us = timeval_subtract_usec(&t1,&t0);
				usec += us;
				usecsquared += us * us;
				--benchmarks;
			}else if(failloc_tests && ret != EXIT_TESTFAILED){
				failloc = failloc_tests; // set it up
			}else if(benchmark_tests && ret != EXIT_TESTFAILED){
				// Throw out the first result -- it tends to be
				// disproportionately slow due to cache effects
				benchmarks = benchmark_tests;
				use_terminfo_color(COLOR_MAGENTA,1);
				printf(" Benchmarking for %jd iterations...\n",benchmarks);
				use_terminfo_color(COLOR_MAGENTA,0);
				if(freopen("/dev/null","w",stdout) == NULL){
					fprintf(stderr," Couldn't redirect stdout\n");
					exit(EXIT_INTERNALEXIT);
				}
			}
		}while(failloc || benchmarks);
		if(benchmark_tests){
			intmax_t avg;
			float stddev;

			avg = usec / benchmark_tests;
			stddev = sqrt((usecsquared * benchmark_tests - (usec * usec)) /
					(benchmark_tests * (benchmark_tests - 1)));
			use_terminfo_color(COLOR_BLUE,1);
			fprintf(stderr," usec Avg: %jd Stddev: %f (%.2f%%)\n",
					avg,stddev,stddev/avg * 100);
			use_terminfo_defcolor();
		}
		exit(ret == EXIT_TESTSUCCESS ? EXIT_HACKTESTSUCCESS : ret);
	}else{ // parent!
		int status = 0;
		pid_t wpid;

		if((wpid = waitpid(pid,&status,0)) != pid){
			printf(" Failure on %u (%s).\n",pid,strerror(errno));
			return EXIT_TESTFAILED;
		}
		kill(-pid,SIGTERM); // Kill everything in the process group
		pid = 0;
		if(WIFEXITED(status)){
			int s = WEXITSTATUS(status);

			if(s == EXIT_HACKTESTSUCCESS){
				return EXIT_TESTSUCCESS;
			}else if(!(s & EXIT_MEMLEAK) && !(s & EXIT_TESTFAILED)){
				printf(" Child exited with unknown status %d.\n",s);
				return EXIT_INTERNALEXIT;
			}
			return s;
		}else if(WIFSIGNALED(status)){
			int sig = WTERMSIG(status);

			printf(" Saw signal %d (%s).\n",sig,strsignal(sig));
			return EXIT_SIGNAL + sig;
		}
		fprintf(stderr," Couldn't interpret child wait() value %d.\n",status);
		return EXIT_TESTFAILED;
	}
}

static unsigned
run_test(const declared_test *dt){
	struct timeval start,end;
	intmax_t usec;
	unsigned ret;

	use_terminfo_color(COLOR_BLUE,1);
	printf("[");
	use_terminfo_color(COLOR_WHITE,1);
	printf("%s",dt->name);
	use_terminfo_color(COLOR_BLUE,1);
	printf("]");
	use_terminfo_defcolor();
	printf(" Running test...");
	Gettimeofday(&start,NULL);
	fflush(stdout);
	fflush(stderr);
	ret = trampoline_test(dt);
	fflush(stdout);
	fflush(stderr);
	Gettimeofday(&end,NULL);
	usec = timeval_subtract_usec(&end,&start);
	use_terminfo_color(COLOR_CYAN,1);
	printf(" %jd.%06jds",usec / 1000000,usec % 1000000);
	use_terminfo_color(COLOR_MAGENTA,1);
	printf(" required to run [%s]. ",dt->name);
	// return code handling is kinda janky but bear with me here:
	//  if it's greater than or equal to EXIT_SIGNAL, subtract EXIT_SIGNAL
	//  to get the signal number. there is no other information available.
	//  we couldn't store much anyway -- we only have bit 6 available.
	//  Otherwise, check the exact values. They could all theoretically be
	//  indepedently set (hence a bitfield) but only MEMLEAK | TESTFAILED
	//  makes sense of all possible combinations.
	//
	//  Note this means you can specify as the expected result ONE of the
	//  following: a particular signal, or a union of result bits --nlb
	if(ret == EXIT_TESTSUCCESS){
		use_terminfo_color(COLOR_GREEN,1);
		printf("Test succeeded.\n");
	}else{
		use_terminfo_color(COLOR_RED,1);
		if(ret >= EXIT_SIGNAL){
			use_terminfo_color(COLOR_RED,1);
			printf("Test aborted due to signal.\n");
			return ret;
		}else if(ret == EXIT_MCHECK){
			printf("Test misused memory.\n");
			return EXIT_MCHECKHACK;
		}else if(ret == EXIT_NEEDMORERAM){
			printf("Test required more RAM than was available.\n");
		}else if(!(ret & (EXIT_MEMLEAK | EXIT_TESTFAILED))){
			printf("Test exit(2)ed directly.\n");
		}
	}
	if(ret & EXIT_MEMLEAK){
		use_terminfo_color(COLOR_YELLOW,1);
		printf("Test leaked memory.\n");
	}
	if(ret & EXIT_TESTFAILED){
		printf("Test returned failure.\n");
	}
	return ret;
}

static void
list_tests(void){
	const declared_test *suite;
	unsigned z,count = 0;

	for(z = 0 ; (suite = BUILTIN_TEST_SUITES[z]) ; ++z){
		testptr fxn;
		unsigned y;

		for(y = 0 ; (fxn = suite[y].testfxn) ; ++y){
			unsigned mb,sec;

			++count;
			mb = suite[y].mb_required;
			sec = suite[y].sec_required;
			mb = mb ? mb : TEST_MEGABYTE_MAX;
			sec = sec ? sec : TEST_RUNTIME_MAX;
			printf("%4u] %30s %10uMb %10us\n",count,
				suite[y].name,mb,sec);
		}
	}
	for(z = 0 ; z < dynamic_test_suite_count ; ++z){
		testptr fxn;
		unsigned y;

		suite = dynamic_test_suites[z];
		for(y = 0 ; (fxn = suite[y].testfxn) ; ++y){
			unsigned mb,sec;

			++count;
			mb = suite[y].mb_required;
			sec = suite[y].sec_required;
			mb = mb ? mb : TEST_MEGABYTE_MAX;
			sec = sec ? sec : TEST_RUNTIME_MAX;
			printf("%4u] %30s %10uMb %10us\n",count,
				suite[y].name,mb,sec);
		}
	}
}

static int
test_should_run(int argc,char * const *argv,const declared_test *test){
	/* if(test->requires_root){
		if(geteuid()){
			return 0;
		}
	} */
	if(argc == 0){
		if(run_all_tests == 0){
			if(test->sec_required > TEST_RUNTIME_MAX){
				return 0;
			}
			if(test->mb_required > TEST_MEGABYTE_MAX){
				return 0;
			}
		}
		return !test->disabled;
	}
	while(*argv){
		if(strcasecmp(*argv,test->name) == 0){
			return 1;
		}
		++argv;
	}
	return 0;
}

static int
run_testsuites(int *wins,int *tests,int *disabled,int argc,char * const *argv,
						const declared_test *suite){
	testptr fxn;
	unsigned y;

	for(y = 0 ; (fxn = suite[y].testfxn) ; ++y){
		unsigned ret,ex;

		if(test_should_run(argc,argv,&suite[y]) == 0){
			++*disabled;
			continue;
		}
		++*tests;
		if(config_dir && Chdir(config_dir)){
			fprintf(stderr,"Error using %s.\n",config_dir);
			return -1;
		}
		ret = run_test(&suite[y]);
		ex = suite[y].expected_result;
		if(ret >= EXIT_SIGNAL || !(ret & EXIT_NEEDMORERAM)){
			if((ret & ex) == ret){
				++*wins;
			}else{
				if(rerun_failed_test_verbosely){
					use_terminfo_color(COLOR_MAGENTA,1);
					printf("\n\nRunning test again, in verbose mode.\n\n\n");
					verbose = 1;
					if(set_log_stdio() == 0){
						run_test(&suite[y]);
					}
				}
				return -1;
			}
		}else{
			// ret & EXIT_NEEDMORERAM is high
			fprintf(stderr,"Ignoring possible failure.\n");
			++*wins;
		}
		if(Chdir(cwd)){
			fprintf(stderr,"Error changing to dir %s.\n",cwd);
			return -1;
		}
	}
	return 0;
}

static void
run_tests(int *wins,int *tests,int *disabled,int argc,char * const *argv){
	const declared_test *suite;
	unsigned z;

	for(z = 0 ; (suite = BUILTIN_TEST_SUITES[z]) ; ++z){
		if(run_testsuites(wins,tests,disabled,argc,argv,suite)){
			return;
		}
	}
	for(z = 0 ; z < dynamic_test_suite_count ; ++z){
		suite = dynamic_test_suites[z];

		if(run_testsuites(wins,tests,disabled,argc,argv,suite)){
			return;
		}
	}
}

static void
usage(const char *name){
	fprintf(stderr,"usage: %s options [tests]\n"
	 " tests: test spec, \"help\" lists. default runs all cheap tests.\n"
	 " -v: be verbose\n"
	 " -r: disable default behavior of verbosely rerunning a failed test\n"
	 " -a: run all tests, including time/space-intensive ones\n"
	 " -c path: set config directory to path\n"
	 " -o path: load shared object at path, and use its tests\n"
	 "Some options are relevant only for certain families of tests:\n"
	 " -- OPTIONS APPLICABLE ONLY TO TESTS EXPECTING SUCCESS --\n"
	 " -f: iteratively test alloc failures\n"
	 " -b[n]: benchmark (mean, variance) over n > 1 iterations (default n = 1000)\n"
	 ,name);
}

static int
handle_args(int argc,char * const *argv){
	const char *DEFAULT_CONF_DIR = "";
	int z,opt;

	if(!Getcwd(cwd,sizeof(cwd))){
		fprintf(stderr,"I couldn't get the cwd, giving up.\n");
		exit(EXIT_FAILURE);
	}
	while((opt = getopt(argc,argv,"c:o:vfb::ar")) != -1){
		switch(opt){
		case 'r':{
			if(verbose){
				fprintf(stderr,"Warning: -r is meaningless with -v.\n");
			}
			rerun_failed_test_verbosely = 0;
			break;
		}case 'o':{
			if(strlen(optarg) >= PATH_MAX){
				fprintf(stderr,"Bad argument to -o: %s\n",optarg);
				return -1;
			}
			if(add_tests_from_so(optarg)){
				return -1;
			}
			break;
		}case 'c':{
			if(config_dir || strlen(optarg) >= PATH_MAX){
				fprintf(stderr,"Invalid use of -c option.\n");
				return -1;
			}
			config_dir = optarg;
			break;
		}case 'v':{
			if(!rerun_failed_test_verbosely){
				fprintf(stderr,"Warning: -r is meaningless with -v.\n");
			}
			if(set_log_stdio()){
				return -1;
			}
			break;
		}case 'f':{
			if(failloc_tests++ || benchmark_tests){
				fprintf(stderr,"Can't use -f option with -f, -b.\n");
				return -1;
			}
			printf("WARNING: -f CAN TAKE HOURS. Failures start at %u.\n",failloc_tests - 1);
			break;
		}case 'b':{
			if(benchmark_tests || failloc_tests){
				fprintf(stderr,"Can't use -b option with -b, -f.\n");
				return -1;
			}
			if(optarg){
				const char *arg = optarg;

				if(lex_u32(&arg,&benchmark_tests)){
					return -1;
				}
			}else{
				benchmark_tests = 1000;
			}
			if(benchmark_tests <= 1){
				fprintf(stderr,"Invalid use of -b option.\n");
				return -1;
			}
			printf("Benchmarking mode; using %u iterations.\n",benchmark_tests);
			break;
		}case 'a':{
			if(run_all_tests++){
				fprintf(stderr,"Invalid use of -a option.\n");
				return -1;
			}
			break;
		}default:{
			return -1;
		} }
	}
	if(config_dir == NULL && strlen(DEFAULT_CONF_DIR)){
		config_dir = DEFAULT_CONF_DIR;
	}
	for(z = optind ; z < argc ; ++z){
		if(strcasecmp(argv[z],"help") == 0){
			list_tests();
			exit(EXIT_SUCCESS);
		}
	}
	return 0;
}

static void
fatal_sig_handler(int signum){
	// propagate the signal out to trampoline process group
	if(pid > 1){
		kill(-pid,signum);
	}
	// ...and then to ourselves (NOT our own group, or we'll kill the shell
	// that spawned us. don't setpgid in the toplevel or we can't kill from
	// within a make process!)
	raise(signum);
}

static int
handle_signals(void){
	struct sigaction sa;

	memset(&sa,0,sizeof(sa));
	sa.sa_handler = fatal_sig_handler;
	sa.sa_flags = SA_RESETHAND | SA_RESTART;
	if(Sigaction(SIGINT,&sa,NULL)){
		return -1;
	}
	return 0;
}

int main(int argc,char * const *argv){
	int tests = 0,wins = 0,disabled = 0;
	struct timeval starttime,endtime;
	const char *xname;
	logctx *lc,lctx;
	intmax_t usec;

	if(libdank_vercheck()){
		return EXIT_FAILURE;
	}
	srand(time(NULL));
	if((xname = strrchr(argv[0],'/')) == NULL){
		xname = argv[0];
	}else{
		++xname;
	}
	lc = &lctx;
	// FIXME if we set this to 1, we coredump in our second test...uhhh
	if(init_logging(lc,NULL,0)){
		goto done;
	}
	if(init_ncurses()){
		goto done;
	}
	Gettimeofday(&starttime,NULL);
	if(handle_args(argc,argv)){
		goto done;
	}
	if(id_cpu()){
		goto done;
	}
	if(handle_signals()){
		goto done;
	}
	init_profiling();
	run_tests(&wins,&tests,&disabled,argc - optind,argv + optind);
	if(argc - optind && tests != argc - optind){
		fprintf(stderr,"Invalid test specified. Available tests are:\n");
		list_tests();
		return EXIT_FAILURE;
	}

done:
	if(tests == 0){
		usage(xname);
		return EXIT_FAILURE;
	}
	Gettimeofday(&endtime,NULL);
	usec = timeval_subtract_usec(&endtime,&starttime);
	use_terminfo_color(wins == tests ? COLOR_GREEN : COLOR_RED,1);
	printf("\nRan %d %s in %jd.%06jds (%d disabled). %d succeeded.\n",tests,
			benchmark_tests ? "benchmarks" : "tests",
			usec / 1000000,usec % 1000000,disabled,wins);
	Free(dynamic_test_suites);
	stop_profiling();
	stop_ncurses();
	free_logctx_ustrings();
	return wins == tests ? EXIT_SUCCESS : EXIT_FAILURE;
}
