#include <libdank/arch/cpu.h>
#include <sys/mman.h>
#include <cunit/cunit.h>
#include <libdank/arch/cpucount.h>
#include <libdank/utils/memlimit.h>

static size_t ASIZE = 0x1000000;

static int
test_limitmmap(void){
	void *head = MAP_FAILED,**oldv = &head;
	size_t tot = 0;

	while((*oldv = Mmalloc("test",ASIZE)) != MAP_FAILED){
		tot += ASIZE;
		printf(" Mmap()ed %zub\n",tot);
		oldv = (void **)*oldv;
	}
	while(head != MAP_FAILED){
		oldv = (void **)*(void **)head;
		Mfree(head,ASIZE);
		head = (void *)oldv;
	}
	return 0;
}

static int
test_limitmalloc(void){
	void *head = NULL,**oldv = &head;
	size_t tot = 0;

	while( (*oldv = Malloc("test",ASIZE)) ){
		tot += ASIZE;
		printf(" Malloc()ed %zub\n",tot);
		oldv = (void **)*oldv;
	}
	while(head){
		oldv = (void **)*(void **)head;
		Free(head);
		head = (void *)oldv;
	}
	return 0;
}

static int
test_limitreallocnull(void){
	void *head = NULL,**oldv = &head;
	size_t tot = 0;

	while( (*oldv = Realloc("test",NULL,ASIZE)) ){
		tot += ASIZE;
		printf(" Realloc()ed %zub\n",tot);
		oldv = (void **)*oldv;
	}
	while(head){
		oldv = (void **)*(void **)head;
		Free(head);
		head = (void *)oldv;
	}
	return 0;
}

static int
test_limitrealloct(void){
	void *t = NULL,*tmp;
	size_t s = 0;

	while( (tmp = Realloc("test",t,s += ASIZE)) ){
		printf(" Realloc()ed %zub\n",s);
		t = tmp;
	}
	Free(t);
	return 0;
}

static int
test_cpucount_main(void){
	unsigned cpu_count;

	if((cpu_count = detect_num_processors()) == 0){
		return -1;
	}
	// Ensure it's a power of 2
	if(cpu_count & (cpu_count - 1)){
		fprintf(stderr," Detected an unlikely %u CPUs\n",cpu_count);
		return -1;
	}
	return 0;
}

static int
test_cpuid_main(void){
	static const size_t usize[] = { 1, 8, 16, 20, 32, 37, 64, 92,
					128, 129, 192, 256, 257, 4096 };
	unsigned z;

	for(z = 0 ; z < sizeof(usize) / sizeof(*usize) ; ++z){
		size_t asize;

		if((asize = align_size(usize[z])) == 0){
			fprintf(stderr," Aligned to 0b: %zub.\n",usize[z]);
			return -1;
		}
		// I know no architecture implementing cache lines > 128b (L2
		// lines on P4) or < 32b. This seems reasonable...
		if(usize[z] >= 32 && asize % 32 && asize % 64 && asize % 128){
			fprintf(stderr," Badly aligned to %zub: %zub.\n",asize,usize[z]);
			return -1;
		}
		if((asize > (usize[z] & 0xffffff80) + 128) || (asize < usize[z])){
			fprintf(stderr," Badly aligned to %zub: %zub.\n",asize,usize[z]);
			return -1;
		}
		printf(" Aligned to %zub: %zub.\n",asize,usize[z]);
	}
	return 0;
}

const declared_test ARCH_TESTS[] = {
	{	.name = "cpucount",
		.testfxn = test_cpucount_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "cpuid",
		.testfxn = test_cpuid_main,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "limitmalloc",
		.testfxn = test_limitmalloc,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "limitmmap",
		.testfxn = test_limitmmap,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "limitrealloct",
		.testfxn = test_limitrealloct,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "limitreallocnull",
		.testfxn = test_limitreallocnull,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
