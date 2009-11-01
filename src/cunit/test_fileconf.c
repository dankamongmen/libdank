#include <cunit/cunit.h>
#include <libdank/modules/fileconf/sbox.h>

#define TEST_CONFIG "testdata"

static int
test_fileconf(void){
	struct config_data *cd;

	if(init_fileconf(NULL)){
		return -1;
	}
	if((cd = open_config(TEST_CONFIG)) == NULL){
		return -1;
	}
	printf(" Prepared config (file) %s\".\n",TEST_CONFIG);
	free_config_data(&cd);
	if(stop_fileconf()){
		return -1;
	}
	return 0;
}

const declared_test FILECONF_TESTS[] = {
	{	.name =  "fileconf",
		.testfxn = test_fileconf,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
