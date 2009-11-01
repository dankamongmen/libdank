#include <cunit/cunit.h>
#include <libdank/utils/string.h>

static const char *haystacks[] = {
	"acgtacgtacgt",
	"acccccct",
	"acgttttacgt",
	NULL
};

static int
test_strcasestr(void){
	const char **haystack;

	for(haystack = haystacks ; *haystack ; ++haystack){
		if(!strcasestr(*haystack,"T")){
			fprintf(stderr," Couldn't find T.\n");
			break;
		}
		if(strcasestr(*haystack,"ttttt")){
			fprintf(stderr," Found ttttt.\n");
			break;
		}
		if(!strcasestr(*haystack,"t")){
			fprintf(stderr," Couldn't find t.\n");
			break;
		}
	}
	if(*haystack){
		fprintf(stderr," Failed: %s.\n",*haystack);
		return -1;
	}
	return 0;
}

const declared_test STRING_TESTS[] = {
	{	.name = "strcasestr",
		.testfxn = test_strcasestr,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
