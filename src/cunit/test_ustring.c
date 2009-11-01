#include <cunit/cunit.h>
#include <libdank/objects/objustring.h>

static char magnumbuf[1024 * 1024];

static int
test_ustringexhaust_main(void){
	ustring u = USTRING_INITIALIZER;

	memset(magnumbuf,'x',sizeof(magnumbuf));
	while(printUString(&u,"%.*s",(int)sizeof(magnumbuf),magnumbuf) > 0){
		;
	}
	reset_ustring(&u);
	return 0;
}

static int
test_ustringfreenull_main(void){
	free_ustring(NULL);
	return 0;
}

const declared_test USTRING_TESTS[] = {
	{	.name = "ustringexhaust",
		.testfxn = test_ustringexhaust_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "ustringfreenull",
		.testfxn = test_ustringfreenull_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
