#include <unistd.h>
#include <cunit/cunit.h>
#include <libdank/utils/dlsym.h>

// Don't make this static, or it won't be able to look itself up with dladdr()
int test_dladdr(void);

int test_dladdr(void){
	void *objs[] = {
		getpid,		// system call
		printf,		// library call
		flog,		// libdank call
		&optarg,	// library variable
				// (don't user errno; it's TLS)
		test_dladdr,	// ourself
		NULL
	},**cur;

	// On FreeBSD, globally-visible functions from dynamic libraries won't
	// return useful data; they'll appear to be _init from the main object.
	for(cur = objs ; *cur ; ++cur){
		Dl_info dli;

		printf(" Looking up ident: %p\n",*cur);
		if(Dladdr(*cur,&dli) == 0){
			fprintf(stderr," Error looking up object!\n");
			return -1;
		}
		printf("  Nearest symbol: %s\n",dli.dli_sname);
		printf("  Nearest symbol address: %p\n",dli.dli_saddr);
		printf("  In object: %s\n",dli.dli_fname);
		printf("  Object load address: %p\n",dli.dli_fbase);
		if(dli.dli_saddr != *cur){
			printf("  Warning: object address mismatch!\n");
		}
	}
	return 0;
}

static int
test_dladdrselfmain(void){
	void *selfobj,*mainptr;
	const char *errstr;
	int ret = -1;
	Dl_info dli;

	if((selfobj = Dlopen(NULL,RTLD_NOW)) == NULL){
		fprintf(stderr," Couldn't open self with dlopen(NULL).\n");
		return -1;
	}
	printf(" Looking up main()\n");
	mainptr = Dlsym(selfobj,"main",&errstr);
	if(errstr){
		fprintf(stderr," Error looking up main()!\n");
		goto done;
	}
	if(Dladdr(mainptr,&dli) == 0){
		fprintf(stderr," Error looking up %p!\n",mainptr);
		goto done;
	}
	printf("  Nearest symbol: %s\n",dli.dli_sname);
	printf("  Nearest symbol address: %p\n",dli.dli_saddr);
	printf("  In object: %s\n",dli.dli_fname);
	printf("  Object load address: %p\n",dli.dli_fbase);
	ret = 0;

done:
	ret |= Dlclose(selfobj);
	return ret;
}

const declared_test DLSYM_TESTS[] = {
	{	.name = "dladdr",
		.testfxn = test_dladdr,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "dladdrselfmain",
		.testfxn = test_dladdrselfmain,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
