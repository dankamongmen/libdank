#include <stdio.h>
#include <cunit/cunit.h>
#include <libdank/utils/magic.h>
#include <libdank/utils/syswrap.h>

static int
test_uintlog2(void){
	uintmax_t exp2;
	unsigned ret,u;

	for(u = 0 ; u < sizeof(uintmax_t) * CHAR_BIT ; ++u){
		exp2 = 1ULL << u;
		printf(" Checking that uintlog2(%ju) == %u.\n",exp2,u);
		if((ret = uintlog2(exp2)) != u){
			fprintf(stderr, " Mismatch! uintlog2(%ju) returned %u.\n",exp2,ret);
			return -1;
		}
	}
	return 0;
}

const declared_test MAGIC_TESTS[] = {
	{	.name = "uintlog2",
		.testfxn = test_uintlog2,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
