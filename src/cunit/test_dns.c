#include <netinet/in.h>
#include <cunit/cunit.h>
#include <libdank/modules/adns/adns.h>

static int
test_rfc1035(void){
	struct in_addr sina;
	int ret = -1;

	if(init_asynchronous_dns()){
		goto done;
	}
	if(synchronous_dnslookup("localhost",&sina)){
		goto done;
	}
	if(sina.s_addr != htonl(INADDR_LOOPBACK)){
		fprintf(stderr," localhost didn't resolve to the loopback address.\n");
		goto done;
	}
	printf(" Verified localhost resolved to the loopback address.\n");
	ret = 0;

done:
	ret |= stop_asynchronous_dns();
	return ret;
}

const declared_test DNS_TESTS[] = {
	{	.name = "rfc1035",
		.testfxn = test_rfc1035,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
