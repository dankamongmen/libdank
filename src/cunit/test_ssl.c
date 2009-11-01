#include <cunit/cunit.h>
#include <libdank/modules/ssl/ssl.h>

static int
test_sslinit(void){
	int ret = -1;

	if(init_ssl()){
		goto done;
	}
	printf(" Initialized secure sockets layer.\n");
	ret = 0;

done:
	ret |= stop_ssl();
	return ret;
}

const declared_test SSL_TESTS[] = {
	{	.name = "sslinit",
		.testfxn = test_sslinit,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
