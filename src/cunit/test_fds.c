#include <fcntl.h>
#include <unistd.h>
#include <cunit/cunit.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/maxfds.h>
#include <libdank/utils/syswrap.h>

static int
test_maxfddup2(void){
	int maxfd,fd,ret = -1;

	if((maxfd = determine_max_fds()) <= 0){
		goto done;
	}
	if((fd = dup2(STDOUT_FILENO,maxfd - 1)) != maxfd - 1){
		goto done;
	}
	printf(" Maximum fd appears to be %d.\n",fd);
	if(Close(fd)){
		goto done;
	}
	ret = 0;

done:
	return ret;
}

static int
test_badfdnodup2(void){
	int maxfd,ret = -1;

	if((maxfd = determine_max_fds()) <= 0){
		goto done;
	}
	if(dup2(STDOUT_FILENO,maxfd) >= 0){
		goto done;
	}
	printf(" Verified unusability of fd %d.\n",maxfd);
	ret = 0;

done:
	return ret;
}

static int
test_unconnected(void){
	int fd,ret = -1;

	if((fd = Socket(AF_INET,SOCK_STREAM,0)) < 0){
		return -1;
	}
	if(!fd_readablep(fd)){
		fprintf(stderr," Unconnected socket was unreadable.\n");
		goto done;
	}
	if(!fd_writeablep(fd)){
		fprintf(stderr," Unconnected socket was unwriteable.\n");
		goto done;
	}
	printf(" Unconnected socket was read/writeable.\n");
	ret = 0;

done:
	Close(fd);
	return ret;
}

static int
test_devzero(void){
	int fd,ret = -1;

	if((fd = Open("/dev/zero",O_RDONLY|O_NONBLOCK)) < 0){
		return -1;
	}
	if(!fd_readablep(fd)){
		fprintf(stderr," /dev/zero fd was unreadable.\n");
		goto done;
	}
	if(!fd_writeablep(fd)){
		fprintf(stderr," /dev/zero fd was unwriteable.\n");
		goto done;
	}
	printf(" /dev/zero fd was read/writeable.\n");
	ret = 0;

done:
	Close(fd);
	return ret;
}

const declared_test FDS_TESTS[] = {
	{	.name = "maxfddup2",
		.testfxn = test_maxfddup2,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "badfdnodup2",
		.testfxn = test_badfdnodup2,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "unconnected",
		.testfxn = test_unconnected,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 1,
	},
	{	.name = "devzero",
		.testfxn = test_devzero,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
