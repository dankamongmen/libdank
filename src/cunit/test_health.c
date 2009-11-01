#include <cunit/cunit.h>
#include <libdank/objects/objustring.h>
#include <libdank/modules/logging/health.h>

static int
test_health(void){
	ustring u = USTRING_INITIALIZER;
	int ret = -1;

	printf(" Testing health monitoring...\n");
	if(stringize_health(&u)){
		goto done;
	}
	printf(" %s\n",u.string);
	ret = 0;

done:
	reset_ustring(&u);
	return ret;
}

const declared_test HEALTH_TESTS[] = {
	{	.name = "health",
		.testfxn = test_health,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
