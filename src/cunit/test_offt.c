#include <fcntl.h>
#include <sys/stat.h>
#include <cunit/cunit.h>
#include <libdank/utils/syswrap.h>

#define TESTFILE "test64"
static int
test_offset64(void){
	struct stat sta;
	int fd,ret = -1;
	// FIXME off_t off;

	if((fd = OpenCreat(TESTFILE,O_RDWR | O_CREAT,755)) < 0){
		goto done;
	}
	if(Fstat(fd,&sta)){
		goto done;
	}
	// FIXME what is to be done with this?
	//off = 0x1ffffffffLL;
	ret = 0;

done:
	if(fd >= 0){
		ret |= Close(fd);
	}
	Unlink(TESTFILE);
	return ret;
}
#undef TESTFILE

const declared_test OFFT_TESTS[] = {
	{	.name = "offset64",
		.testfxn = test_offset64,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
