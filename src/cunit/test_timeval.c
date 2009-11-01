#include <libdank/utils/time.h>
#include <cunit/cunit.h>
#include <libdank/utils/syswrap.h>

static int
test_timesubsanity(void){
	struct timeval minuend,subtrahend;
	unsigned x;

	printf(" Testing time's arrow...");
	fflush(stdout);
	if(Gettimeofday(&minuend,NULL)){
		return -1;
	}
	for(x = 0 ; x < 100000 ; ++x){
		if(Gettimeofday(&subtrahend,NULL)){
			return -1;
		}
		timeval_subtract_usec(&minuend,&subtrahend);
		if(minuend.tv_sec > subtrahend.tv_sec){
			fprintf(stderr,"Time went backwards!\n");
			return -1;
		}
	}
	printf(" Mmmm, we cooked delicious entropy.\n");
	return 0;
}

static int
test_timevalsub(void){
	struct tvpair {
		struct timeval minuend;
		struct timeval subtrahend;
		intmax_t usec_delta;
	} tvpairs[] = {
		{
			.minuend = { .tv_sec = 1, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 2, .tv_usec = 0, },
			.usec_delta = -1000000,
		},{
			.minuend = { .tv_sec = 2, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 0, },
			.usec_delta = 1000000,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 999999, },
			.usec_delta = -999999,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 999999, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 0, },
			.usec_delta = 999999,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 1, },
			.usec_delta = -1,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 1, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 0, },
			.usec_delta = 1,
		},{
			.minuend = { .tv_sec = 0, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 0, .tv_usec = 0, },
			.usec_delta = 0,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 999999, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 999999, },
			.usec_delta = 0,
		},{
			.minuend = { .tv_sec = 0, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 1, .tv_usec = 999999, },
			.usec_delta = -1999999,
		},{
			.minuend = { .tv_sec = 1, .tv_usec = 999999, },
			.subtrahend = { .tv_sec = 0, .tv_usec = 0, },
			.usec_delta = 1999999,
		},{
			.minuend = { .tv_sec = 0, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 999999, .tv_usec = 999999, },
			.usec_delta = -999999999999LL,
		},{
			.minuend = { .tv_sec = 999999, .tv_usec = 999999, },
			.subtrahend = { .tv_sec = 0, .tv_usec = 0, },
			.usec_delta = 999999999999LL,
		},{
			.minuend = { .tv_sec = 0, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 999999, .tv_usec = 0, },
			.usec_delta = -999999000000LL,
		},{
			.minuend = { .tv_sec = 999999, .tv_usec = 0, },
			.subtrahend = { .tv_sec = 0, .tv_usec = 0, },
			.usec_delta = 999999000000LL,
		},
	};
	size_t i;

	for(i = 0 ; i < sizeof(tvpairs) / sizeof(*tvpairs) ; ++i){
		struct timeval s = tvpairs[i].subtrahend;
		struct timeval m = tvpairs[i].minuend;
		intmax_t ret;

		if((ret = timeval_subtract_usec(&m,&s)) != tvpairs[i].usec_delta){
			fprintf(stderr," Got diff of %jd, wanted %jd\n",ret,tvpairs[i].usec_delta);
			return -1;
		}
		printf(" Checked %jd == %jd.\n",ret,tvpairs[i].usec_delta);
	}
	return 0;
}

const declared_test TIMEVAL_TESTS[] = {
	{	.name = "timevalsub",
		.testfxn = test_timevalsub,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "timesubsanity",
		.testfxn = test_timesubsanity,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
