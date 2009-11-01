#include <cunit/cunit.h>
#include <libdank/modules/logging/logging.h>

static int
test_loggingmain(void){
	nag("\tThis is only a test.\n");
	bitch("\tThis is only a test.\n");
	moan("\tThis is only a test.\n");
	return 0;
}

const declared_test LOGCTX_TESTS[] = {
	{	.name = "logctx",
		.testfxn = test_loggingmain,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
