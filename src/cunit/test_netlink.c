#include <netinet/in.h>
#include <cunit/cunit.h>
#include <libdank/ersatz/compat.h>
#include <libdank/modules/netlink/netlink.h>

static int
test_netlink(void){
	ustring u = USTRING_INITIALIZER;
	int ret = -1;

	if(init_netlink_layer()){
		goto done;
	}
	if(stringize_netinfo(&u)){
		goto done;
	}
	printf("%s",u.string);
	ret = 0;

done:
	ret |= stop_netlink_layer();
	reset_ustring(&u);
	return 0;
}

static int
test_netlinklocal(void){
	uint32_t ip = INADDR_LOOPBACK;
	int ret = -1;

	if(init_netlink_layer()){
		return -1;
	}
	if(!ip_is_local(ip)){
		goto done;
	}
	ret = 0;

done:
	ret |= stop_netlink_layer();
	return 0;
}

static int
test_netlinkmtu(void){
	unsigned mmtu;
	int ret = -1;

	if(init_netlink_layer()){
		return -1;
	}
	if((mmtu = get_maximum_mtu()) == 0){
		goto done;
	}
	printf(" Maximum MTU: %u\n",mmtu);
	ret = 0;

done:
	ret |= stop_netlink_layer();
	return 0;
}

const declared_test NETLINK_TESTS[] = {
	{	.name = "netlink",
		.testfxn = test_netlink,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "netlinklocal",
		.testfxn = test_netlinklocal,
#ifdef LIB_COMPAT_LINUX
		.expected_result = EXIT_TESTSUCCESS,
#else
		.expected_result = EXIT_TESTFAILED,
#endif
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "netlinkmtu",
		.testfxn = test_netlinkmtu,
#ifdef LIB_COMPAT_LINUX
		.expected_result = EXIT_TESTSUCCESS,
#else
		.expected_result = EXIT_TESTFAILED,
#endif
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
