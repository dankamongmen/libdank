#include <cunit/cunit.h>

static int
test_extensionexample(void){
	printf(" I was loaded from an extension module.\n");
	return 0;
}

const declared_test EXTENSIONEXAMPLE_TESTS[] = {
	{	.name = "extensionexample",
		.testfxn = test_extensionexample,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};

const char *CUNIT_EXTENSIONS[] = {
	"EXTENSIONEXAMPLE_TESTS",
	NULL
};
