#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cunit/cunit.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/ctlserver/ctlserver.h>

static int
test_ctlserver(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	printf(" Successfully started control server.\n");
	ret = 0;

done:
	if(stop_ctlserver() == 0){
		printf(" Successfully stopped control server.\n");
	}else{
		ret = -1;
	}
	return ret;
}

int ctlclient_quiet(const char *cmd){
	int status;
	pid_t pid;

	if((pid = ctlclient_quiet_nowait(cmd)) > 0){
		if(Waitpid(pid,&status,0) != pid){
			fprintf(stderr," Couldn't wait on child.\n");
		}else if(!WIFEXITED(status)){
			fprintf(stderr," Child exited abnormally.\n");
		}else if(WEXITSTATUS(status)){
			fprintf(stderr," Child returned failure (%d).\n",WEXITSTATUS(status));
		}else{
			return 0;
		}
	}
	return -1;
}

pid_t ctlclient_quiet_nowait(const char *cmd){
	pid_t pid;

	fflush(stdout);
	fflush(stderr);
	if((pid = Fork()) == 0){ // client
		char CUNIT_CTLCLIENT[] = "bin/crosier-nullin";
		char SERVER[] = CUNIT_CTLSERVER;
		char *cmddup = Strdup(cmd);
		char * const argv[] = {
			CUNIT_CTLCLIENT,
			SERVER,
			cmddup,
			NULL
		};
		int i;

		if(Setpgid(0,0)){
			_exit(EXIT_FAILURE);
		}
		// FIXME without these, unit tests hang...why? what is fd 4? it
		// is the fd we accept(2) off the PF_UNIX socket...unsolvable
		// race condition (see receive_fd() in libcrosier) :/
		for(i = 4 ; i < 0xff ; ++i){
			int flags;

			if((flags = fcntl(i,F_GETFD)) >= 0){
				if(!(flags & FD_CLOEXEC)){
					nag("BLEEDING FD %d!\n",i);
					Close(i);
				}
			}else{
				close(i);
			}
		}
		Execvp(CUNIT_CTLCLIENT,argv);
		Free(cmddup);
		_exit(EXIT_FAILURE);
	}
	return pid;
}

static int
test_ctlserver_internal(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	printf(" Testing internal no-op CTLserver path...\n");
	ret = ctlclient_quiet("internal_noop");

done:
	ret |= stop_ctlserver();
	return ret;
}

static int
test_ctlserver_external(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	printf(" Testing external no-op CTLserver path...\n");
	ret = ctlclient_quiet("external_noop");

done:
	ret |= stop_ctlserver();
	return ret;
}

static int
test_ctlserver_help(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	printf(" Testing external help CTLserver path...\n");
	ret = ctlclient_quiet("help");

done:
	ret |= stop_ctlserver();
	return ret;
}

static int
test_ctlserver_memdump(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	if(init_log_server()){
		goto done;
	}
	printf(" Testing external mem_dump CTLserver path...\n");
	ret = ctlclient_quiet("mem_dump");
	printf("\n");

done:
	ret |= stop_log_server();
	ret |= stop_ctlserver();
	return ret;
}

static int
test_ctlserver_noop_repeat(void){
	char SERVER[] = CUNIT_CTLSERVER;
	#define CONNCOUNT 4
	#define CONNCMD "external_noop"
	int ret = -1,i;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	printf(" Connecting %d times for %s...\n",CONNCOUNT,CONNCMD);
	for(i = 0 ; i < CONNCOUNT ; ++i){
		if(ctlclient_quiet(CONNCMD)){
			goto done;
		}
	}
	ret = 0;

done:
	ret |= stop_ctlserver();
	return ret;
	#undef CONNCMD
	#undef CONNCOUNT
}

static int
test_ctlserver_unlinkrecovery(void){
	char SERVER[] = CUNIT_CTLSERVER;
	int ret = -1;

	if(init_ctlserver(SERVER)){
		goto done;
	}
	if(Unlink(SERVER)){
		goto done;
	}
	printf(" Testing CTLserver socket unlink recovery...\n");
	ret = ctlclient_quiet("internal_noop");

done:
	ret |= stop_ctlserver();
	return ret;
}

const declared_test CTLSERVER_TESTS[] = {
	{	.name = "ctlserver",
		.testfxn = test_ctlserver,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-internal",
		.testfxn = test_ctlserver_internal,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-external",
		.testfxn = test_ctlserver_external,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-help",
		.testfxn = test_ctlserver_help,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-memdump",
		.testfxn = test_ctlserver_memdump,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-noop-repeat",
		.testfxn = test_ctlserver_noop_repeat,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ctlserver-unlinkrecovery",
		.testfxn = test_ctlserver_unlinkrecovery,
		.expected_result = EXIT_TESTFAILED | EXIT_MEMLEAK,
		.sec_required = 0, .mb_required = 0, .disabled = 1, // FIXME
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
