#include <unistd.h>
#include <cunit/cunit.h>
#include <libdank/utils/procfs.h>
#include <libdank/utils/memlimit.h>

static int
test_proccmdline(void){
	char *argv0;
	pid_t pid;

	pid = getpid();
	printf(" Checking /proc/%d for cmdline...\n",pid);
	if((argv0 = procfs_cmdline_argv0(pid)) == NULL){
		fprintf(stderr," Couldn't read /proc entry.\n");
		return -1;
	}
	printf(" Got argv[0]: %s\n",argv0);
	Free(argv0);
	return 0;
}

const declared_test PROCFS_TESTS[] = {
	{	.name = "proccmdline",
		.testfxn = test_proccmdline,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
