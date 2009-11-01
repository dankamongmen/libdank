#include <cunit/cunit.h>
#include <libdank/utils/vm.h>

static int
test_hugepagesize(void){
	size_t psize;
	int ret = -1;

	printf(" Testing huge page support detection...\n");
	if((psize = get_max_pagesize()) < 4096){ // check for absurd values
		fprintf(stderr," Error detecting maximum page size.\n");
		goto done;
	}
	printf(" Detected maximum page size of %zub.\n",psize);
	ret = 0;

done:
	return ret;
}

const declared_test VM_TESTS[] = {
	{	.name = "hugepagesize",
		.testfxn = test_hugepagesize,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
