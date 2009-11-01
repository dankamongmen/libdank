#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <cunit/cunit.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/objustring.h>

static int
test_successcase(void){
	printf(" This test should succeed.\n");
	return 0;
}

static int
test_failurecase(void){
	printf(" This test should fail.\n");
	return -1;
}

static int
test_memleak(void){
	if(Malloc("memory leak",1) == NULL){
		return -1;
	}
	printf(" This test should leak memory.\n");
	return 0;
}

static int
test_sigsegv(void){
	printf(" This test should segmentation fault.\n");
	return raise(SIGSEGV);
}

static int __attribute__ ((noreturn))
test_timer(void){
	printf(" This test should run out of time.\n");
	alarm(1);
	for( ; ; );
	exit(0);
}

static int __attribute__ ((noreturn))
test_exitzero(void){
	printf(" This test should internally exit with 0.\n");
	_exit(0);
}

static int __attribute__ ((noreturn))
test_exitone(void){
	printf(" This test should internally exit with 1.\n");
	_exit(1);
}

static int
test_limitfailloc(void){
	const int64_t FAILLOC_ON = 3;
	const size_t ASIZE = 1;
	unsigned z;
	void *v;

	failloc_on_n(FAILLOC_ON);
	for(z = 0 ; z < FAILLOC_ON ; ++z){
		if((v = Malloc("failloc tester",ASIZE)) == NULL){
			goto err;
		}
		Free(v);
	}
	if( (v = Malloc("failloc tester",ASIZE)) ){
		goto err;
	}
	// test reset behavior
	failloc_on_n(-1LL);
	if((v = Malloc("failloc tester",ASIZE)) == NULL){
		goto err;
	}
	Free(v);
	printf(" Alloc failed on %u with deallocs.\n",z);
	failloc_on_n(FAILLOC_ON);
	for(z = 0 ; z < FAILLOC_ON ; ++z){
		if((v = Malloc("failloc tester",ASIZE)) == NULL){
			goto err;
		}
	}
	if( (v = Malloc("failloc tester",ASIZE)) ){
		goto err;
	}
	printf(" Alloc failed on %u without deallocs.\n",z);
	return 0;

err:
	printf(" Failure on allocation %u (%p)\n",z,v);
	return -1;
}

const declared_test CUNIT_TESTS[] = {
	{	.name = "successcase",
		.testfxn = test_successcase,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "failurecase",
		.testfxn = test_failurecase,
		.expected_result = EXIT_TESTFAILED,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "memleak",
		.testfxn = test_memleak,
		.expected_result = EXIT_MEMLEAK,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "sigsegv",
		.testfxn = test_sigsegv,
		.expected_result = EXIT_SIGNAL + SIGSEGV,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "timer",
		.testfxn = test_timer,
		.expected_result = EXIT_SIGNAL + SIGALRM,
		.sec_required = 1,
		.mb_required = 0, .disabled = 0,
	},
	{	.name = "internalexitzero",
		.testfxn = test_exitzero,
		.expected_result = EXIT_INTERNALEXIT,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "internalexitone",
		.testfxn = test_exitone,
		.expected_result = EXIT_INTERNALEXIT,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "limitfailloc",
		.testfxn = test_limitfailloc,
		.expected_result = EXIT_MEMLEAK,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
