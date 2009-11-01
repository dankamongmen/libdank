#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cunit/cunit.h>
#include <libdank/utils/mmap.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/ringbufs.h>
#include <libdank/objects/filewindow.h>

static int
checkchars(const unsigned char *buf,unsigned c,size_t len){
	size_t z;

	for(z = 0 ; z < len ; ++z){
		if(buf[z] != c){
			fprintf(stderr," Bad character at %p:%zu (%p) (got 0x%x, wanted 0x%x)\n",
					buf,z,buf + z,buf[z],c);
			return -1;
		}
	}
	return 0;
}

static int
test_mremap(int fd,int mflags,size_t firstlen,size_t nextlen){
	void *ret,*newret;

	if((ret = mremap_and_truncate(fd,NULL,0,firstlen,PROT_WRITE|PROT_READ,mflags)) == MAP_FAILED){
		fprintf(stderr," Couldn't get primary mremap() request.\n");
		return -1;
	}
	printf(" %zub buffer mapped at %p.\n",(size_t)firstlen,ret);
	memset(ret,0xa5,firstlen); // 1010 0101
	if(checkchars(ret,0xa5,firstlen)){
		mremap_munmap(ret,firstlen);
		return -1;
	}
	printf(" Verified %zu 0x%x's.\n",firstlen,0xa5);
	if((newret = mremap_and_truncate(fd,ret,firstlen,nextlen,PROT_WRITE|PROT_READ,mflags)) == MAP_FAILED){
		fprintf(stderr," Couldn't get secondary mremap() request.\n");
		mremap_munmap(ret,firstlen);
		return -1;
	}
	printf(" %zub buffer mapped at %p.\n",(size_t)nextlen,newret);
	if(checkchars(newret,0xa5,firstlen)){
		mremap_munmap(newret,nextlen);
		return -1;
	}
	printf(" Verified %zu 0x%x's.\n",firstlen,0xa5);
	memset((char *)newret + firstlen,0x69,nextlen - firstlen); // 1001 0110
	if(checkchars((const unsigned char *)newret + firstlen,0x69,nextlen - firstlen)){
		mremap_munmap(newret,nextlen);
		return -1;
	}
	printf(" Verified %zu 0x%x's.\n",nextlen - firstlen,0x69);
	return mremap_munmap(newret,nextlen);
}

static int
test_mremap_anon(int mflag){
	size_t sizes[][2] = {
		{ 4096, 8192, },
		{ 4096, 16777216, },
	};
	unsigned z;

	for(z = 0 ; z < sizeof(sizes) / sizeof(*sizes) ; ++z){
		if(test_mremap(-1,MAP_ANON | mflag,sizes[z][0],sizes[z][1])){
			return -1;
		}
	}
	return 0;
}

static int
test_mremap_private_anon(void){
	return test_mremap_anon(MAP_PRIVATE);
}

static int
test_mremap_shared_anon(void){
	return test_mremap_anon(MAP_SHARED);
}

// MAP_SHARED breaks on Linux using native mremap(2)...why? see bug 733
static int
test_mremap_zero(int mflag){
	size_t sizes[][2] = {
		{ 4096, 8192, },
		{ 4096, 16777216, },
	};
	unsigned z;

	int fd,ret;

	if((fd = Open("/dev/zero",O_RDWR)) < 0){
		fprintf(stderr," Couldn't open /dev/zero.\n");
		return -1;
	}
	for(z = 0 ; z < sizeof(sizes) / sizeof(*sizes) ; ++z){
		if( (ret = test_mremap(fd,mflag,sizes[z][0],sizes[z][1])) ){
			break;
		}
	}
	ret |= Close(fd);
	return ret;
}

static int
test_mremap_private_zero(void){
	return test_mremap_zero(MAP_PRIVATE);
}

static int
test_mremap_shared_zero(void){
	return test_mremap_zero(MAP_SHARED);
}

static int
test_split_munmap(int fd,int mflags,size_t s1,size_t s2){
	unsigned char *ret;
	size_t loff;

	if((ret = Mmap(NULL,s2,PROT_WRITE|PROT_READ,mflags,fd,0)) == MAP_FAILED){
		fprintf(stderr," Couldn't get primary mmap() request.\n");
		return -1;
	}
	printf(" %zub buffer mapped at %p.\n",s2,ret);
	for(loff = 0 ; loff < s2 ; loff += s1){
		memset(ret + loff,0xa5,s2 - loff); // 1010 0101
		if(munmap(ret + loff,s1)){
			bitch("Couldn't munmap %zu at %p.\n",s1,ret + loff);
			return -1;
		}
	}
	if(Munmap(ret,s2)){
		return -1;
	}
	return 0;
}

// MAP_SHARED breaks on Linux using native mremap(2)...why? see bug 733
static int
test_mmap_split_munmap(int mflag){
	size_t sizes[][2] = {
		{ 4096, 65536, },
		{ 65536, 16777216, },
	};
	unsigned z;

	int fd,ret;

	if((fd = Open("/dev/zero",O_RDWR)) < 0){
		fprintf(stderr," Couldn't open /dev/zero.\n");
		return -1;
	}
	for(z = 0 ; z < sizeof(sizes) / sizeof(*sizes) ; ++z){
		if( (ret = test_split_munmap(fd,mflag,sizes[z][0],sizes[z][1])) ){
			break;
		}
	}
	ret |= Close(fd);
	return ret;
}

static int
test_mmap_split_private_munmap(void){
	return test_mmap_split_munmap(MAP_PRIVATE);
}

static int
test_mmap_split_shared_munmap(void){
	return test_mmap_split_munmap(MAP_SHARED);
}

static int
test_mmap_window(void){
	int fd = -1,ret = -1;
	size_t len = 4096,z;
	mmap_window mw;

	if((fd = open("/dev/zero",O_RDONLY)) < 0){
		return -1;
	}
	if(initialize_mmap_window(&mw,fd,PROT_READ,len)){
		close(fd);
		return -1;
	}
	for(z = 0 ; z < len ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(extend_mmap_window(&mw,fd,PROT_READ,len * 2)){
		goto done;
	}
	for(z = 0 ; z < len * 2 ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(shrink_mmap_window(&mw,len)){
		goto done;
	}
	for(z = len ; z < len * 2 ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(reset_mmap_window(&mw,fd,PROT_READ)){
		goto done;
	}
	ret = 0;

done:
	ret |= close(fd);
	ret |= release_mmap_window(&mw);
	return ret;
}

static int
test_mmap_window_anonymous(void){
	int ret = -1;
	size_t len = 4096,z;
	mmap_window mw;

	if(initialize_mmap_window(&mw,-1,PROT_READ|PROT_WRITE,len)){
		return -1;
	}
	for(z = 0 ; z < len ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(extend_mmap_window(&mw,-1,PROT_READ|PROT_WRITE,len * 2)){
		goto done;
	}
	for(z = 0 ; z < len * 2 ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(shrink_mmap_window(&mw,len)){
		goto done;
	}
	for(z = len ; z < len * 2 ; ++z){
		if(mmap_window_charat(&mw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(reset_mmap_window(&mw,-1,PROT_READ|PROT_WRITE)){
		goto done;
	}
	ret = 0;

done:
	ret |= release_mmap_window(&mw);
	return ret;
}

static int
test_scratchfile_window(void){
	int fd = -1,ret = -1;
	size_t len = 4096,z;
	scratchfile_window sw;

	if((fd = open("/dev/zero",O_RDONLY)) < 0){
		return -1;
	}
	if(initialize_scratchfile_window(&sw,fd,PROT_READ,len)){
		close(fd);
		return -1;
	}
	for(z = 0 ; z < len ; ++z){
		if(scratchfile_window_charat(&sw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(extend_scratchfile_window(&sw,PROT_READ,len * 2)){
		goto done;
	}
	for(z = 0 ; z < len * 2 ; ++z){
		if(scratchfile_window_charat(&sw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(shrink_scratchfile_window(&sw,len)){
		goto done;
	}
	for(z = len ; z < len * 2 ; ++z){
		if(scratchfile_window_charat(&sw,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(reset_scratchfile_window(&sw,PROT_READ)){
		goto done;
	}
	ret = 0;

done:
	ret |= close(fd);
	ret |= release_scratchfile_window(&sw);
	return ret;
}

static int
test_scratchfile_ring(void){
	int fd = -1,ret = -1;
	size_t len = 4096,z;
	scratchfile_ring sr;

	if((fd = open("/dev/zero",O_RDONLY)) < 0){
		return -1;
	}
	if(initialize_scratchfile_ring(&sr,fd,PROT_READ,len)){
		close(fd);
		return -1;
	}
	for(z = 0 ; z < len ; ++z){
		if(scratchfile_ring_charat(&sr,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(extend_scratchfile_ring(&sr,PROT_READ,len * 2)){
		goto done;
	}
	for(z = 0 ; z < len * 2 ; ++z){
		if(scratchfile_ring_charat(&sr,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(shrink_scratchfile_ring(&sr,len)){
		goto done;
	}
	for(z = len ; z < len * 2 ; ++z){
		if(scratchfile_ring_charat(&sr,z)){
			fprintf(stderr," Invalid contents.\n");
			goto done;
		}
	}
	if(reset_scratchfile_ring(&sr,PROT_READ)){
		goto done;
	}
	ret = 0;

done:
	ret |= close(fd);
	ret |= release_scratchfile_ring(&sr);
	return ret;
}

const declared_test MMAP_TESTS[] = {
	{	.name = "mremap_private_anon",
		.testfxn = test_mremap_private_anon,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mremap_shared_anon",
		.testfxn = test_mremap_shared_anon,
		.expected_result = EXIT_TESTSUCCESS | (EXIT_SIGNAL + SIGBUS),
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mremap_private_zero",
		.testfxn = test_mremap_private_zero,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mremap_shared_zero",
		.testfxn = test_mremap_shared_zero,
		.expected_result = EXIT_TESTSUCCESS | (EXIT_SIGNAL + SIGBUS),
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mmap_split_private_munmap",
		.testfxn = test_mmap_split_private_munmap,
		.expected_result = EXIT_TESTSUCCESS | (EXIT_SIGNAL + SIGBUS),
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mmap_split_shared_munmap",
		.testfxn = test_mmap_split_shared_munmap,
		.expected_result = EXIT_TESTSUCCESS | (EXIT_SIGNAL + SIGBUS),
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mmap_window",
		.testfxn = test_mmap_window,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "mmap_window_anonymous",
		.testfxn = test_mmap_window_anonymous,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "scratchfile_window",
		.testfxn = test_scratchfile_window,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "scratchfile_ring",
		.testfxn = test_scratchfile_ring,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
