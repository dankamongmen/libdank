#include <arpa/inet.h>
#include <cunit/cunit.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/rfc3330.h>

static int
test_rfc3330(void){
	const struct {
		const char *ip;
		rfc3330_type expecttype;
	} tests[] = {
		{ 	.ip = "0.0.0.0",	
			.expecttype = RFC3330_THISNETWORK,	},
		{	.ip = "0.255.255.255",
			.expecttype = RFC3330_THISNETWORK,	},
		{	.ip = "1.0.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "9.255.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "10.0.0.0",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "10.255.225.255",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "11.0.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "13.255.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "14.0.0.0",
			.expecttype = RFC3330_PUBLICDATANET,	},
		{	.ip = "14.255.255.255",
			.expecttype = RFC3330_PUBLICDATANET,	},
		{	.ip = "15.0.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "126.255.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "127.0.0.0",
			.expecttype = RFC3330_LOOPBACK,		},
		{	.ip = "127.255.255.255",
			.expecttype = RFC3330_LOOPBACK,		},
		{	.ip = "128.0.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "169.253.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "169.254.0.0",
			.expecttype = RFC3330_LINKLOCAL,	},
		{	.ip = "169.254.255.255",
			.expecttype = RFC3330_LINKLOCAL,	},
		{	.ip = "169.255.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "172.15.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "172.16.0.0",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "172.31.255.255",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "172.32.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "192.167.255.255",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = "192.168.0.0",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "192.168.255.255",
			.expecttype = RFC3330_PRIVATE,		},
		{	.ip = "192.169.0.0",
			.expecttype = RFC3330_UNCLASSIFIED,	},
		{	.ip = NULL,
			.expecttype = 0,			}
	},*curtest;

	for(curtest = tests ; curtest->ip ; ++curtest){
		rfc3330_type ret;
		uint32_t ip;

		printf(" Checking IPv4 %s...",curtest->ip);
		fflush(stdout);
		if(Inet_pton(AF_INET,curtest->ip,&ip) < 0){
			return -1;
		}
		if((ret = categorize_ipv4address(ntohl(ip))) != curtest->expecttype){
			fprintf(stderr," expected type %d, got %d.\n",curtest->expecttype,ret);
			return -1;
		}
		printf("got expected type %d.\n",ret);
	}
	return 0;
}

const declared_test RFC3330_TESTS[] = {
	{
		.name = "rfc3330",
		.testfxn = test_rfc3330,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},{
		.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
