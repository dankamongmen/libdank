#include <libdank/utils/hex.h>
#include <cunit/cunit.h>

static const struct {
	const char *hex;
	const char *ascii;
	size_t reslen;
} tests[] = {
	{ .hex = "",		.ascii = "",		.reslen = 0, },
	{ .hex = "00",		.ascii = "\x0",		.reslen = 1, },
	{ .hex = "0000",	.ascii = "\0\0",	.reslen = 2, },
	{ .hex = "ff",		.ascii = "\xff",	.reslen = 1, },
	{ .hex = "FF",		.ascii = "\xff",	.reslen = 1, },
	{ .hex = "00ab",	.ascii = "\x0\xab",	.reslen = 2, },
	{ .hex = "ab00",	.ascii = "\xab\x0",	.reslen = 2, },
	{ .hex = "ff0F",	.ascii = "\xff\xf",	.reslen = 2, },
	{ .hex = "F0e0",	.ascii = "\xf0\xe0",	.reslen = 2, },
	{ .hex = NULL,		.ascii = NULL,		.reslen = 0, },
};

static const struct {
	const char *hex;
	size_t reslen;
} badtests[] = {
	{ .hex = "",		.reslen = 1, },
	{ .hex = "0",		.reslen = 1, },
	{ .hex = "00",		.reslen = 2, },
	{ .hex = "FF",		.reslen = 2, },
	{ .hex = "00f",		.reslen = 2, },
	{ .hex = "000",		.reslen = 2, },
	{ .hex = "ff0",		.reslen = 2, },
	{ .hex = "ff 0f",	.reslen = 2, },
	{ .hex = "FF ff",	.reslen = 2, },
	{ .hex = "00 ab",	.reslen = 2, },
	{ .hex = "ff 0F",	.reslen = 2, },
	{ .hex = NULL,		.reslen = 0, },
};

static int
test_hextoascii(void){
	const typeof (*tests) *test;
	unsigned char buf[80];

	for(test = tests ; test->hex ; ++test){
		printf(" Testing \"%s\": ",test->hex);
		if(hextoascii(test->hex,buf,EOF,test->reslen) == NULL){
			printf(" Failure.\n");
			return -1;
		}else if(memcmp(test->ascii,buf,test->reslen)){
			printf(" Failure.\n");
			return -1;
		}
		printf(" Success.\n");
	}
	return 0;
}

static int
test_badhextoascii(void){
	const typeof (*badtests) *test;
	unsigned char buf[80];

	for(test = badtests ; test->hex ; ++test){
		printf(" Testing \"%s\": ",test->hex);
		if(hextoascii(test->hex,buf,EOF,test->reslen)){
			printf(" Failure.\n");
			return -1;
		}
		printf(" Success.\n");
	}
	return 0;
}

static int
test_asciitohex(void){
	const typeof (*tests) *test;
	char buf[80];

	for(test = tests ; test->ascii ; ++test){
		printf(" Testing \"%s\": ",test->hex);
		asciitohex(test->ascii,buf,EOF,test->reslen);
		if(strcasecmp(test->hex,buf)){
			printf(" Failure (got %s).\n",buf);
			return -1;
		}
		printf(" Success.\n");
	}
	return 0;
}

static int
test_us_asciitohex(void){
	const typeof (*tests) *test;

	for(test = tests ; test->ascii ; ++test){
		ustring us = USTRING_INITIALIZER;

		if(us_asciitohex(test->ascii,&us,EOF,test->reslen)){
			fprintf(stderr," Failed to stringize hex.\n");
			return -1;
		}
		if(us.string){
			if(strcasecmp(test->hex,us.string)){
				printf(" Failure (got %s).\n",us.string);
				reset_ustring(&us);
				return -1;
			}
		}else if(strlen(test->hex)){
			printf(" Failure (no conversion).\n");
			reset_ustring(&us);
			return -1;
		}
		reset_ustring(&us);
		printf(" Success.\n");
	}
	return 0;
}

const declared_test HEX_TESTS[] = {
	{	.name = "hextoascii",
		.testfxn = test_hextoascii,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "badhextoascii",
		.testfxn = test_badhextoascii,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "asciitohex",
		.testfxn = test_asciitohex,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "us_asciitohex",
		.testfxn = test_us_asciitohex,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
