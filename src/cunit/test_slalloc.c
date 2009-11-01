#include <stddef.h>
#include <cunit/cunit.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/slalloc.h>
#include <libdank/objects/objustring.h>

static int
test_slallocnullkill(void){
	printf(" Verifying safe destruction of NULL slalloc...\n");
	destroy_slalloc(NULL);
	return 0;
}

static int
test_slallocspringup(void){
	struct slalloc *sl;

	printf(" Constructing slalloc...\n");
	if((sl = create_slalloc(sizeof(sl))) == NULL){
		return -1;
	}
	printf(" Destroying slalloc...\n");
	destroy_slalloc(sl);
	return 0;
}

static int
test_slallocshutitdown(void){
	struct slalloc *sl;

	printf(" Trying to force a 0-byte slalloc...\n");
	if( (sl = create_slalloc(0)) ){
		fprintf(stderr," Created!\n");
		destroy_slalloc(sl);
		return -1;
	}
	return 0;
}

struct nastybuf {
	char badalign[55];
};

struct bigbuf {
	char sobigtheycallitdinosaur[1911];
};

struct obesebuf {
	char biggerthanwildthing[31337];
};

typedef struct bigbuf bigbuf;
typedef struct nastybuf nastybuf;
typedef struct obesebuf obesebuf;

DEFINE_SLALLOC_CACHE(int);
DEFINE_SLALLOC_CACHE(nastybuf);
DEFINE_SLALLOC_CACHE(bigbuf);
DEFINE_SLALLOC_CACHE(obesebuf);

static int
test_slallocloavesandints(void){
	ustring u = USTRING_INITIALIZER;
	struct slalloc_int *sl;
	int ret = -1;
	unsigned a;
	int *erp;

	printf(" Constructing <int> slalloc...\n");
	if((sl = create_slalloc_int()) == NULL){
		return -1;
	}
#define ALLOCATION_COUNT 0x100000
	printf(" Allocating %d <int>s from slalloc...\n",ALLOCATION_COUNT);
	for(a = 0 ; a < ALLOCATION_COUNT ; ++a){
		if((erp = slalloc_int_new(sl)) == NULL){
			goto done;
		}
	}
#undef ALLOCATION_COUNT
	if(stringize_slalloc_int(&u,sl)){
		goto done;
	}
	printf(" %s\n",u.string);
	reset_ustring(&u);
	ret = 0;
	
done:
	printf(" Destroying <int> slalloc...\n");
	destroy_slalloc_int(sl);
	return ret;
}

static int
test_slallocinttracking(void){
	ustring u = USTRING_INITIALIZER;
#define ALLOCATION_COUNT 0x100000
	struct slalloc_int *sl;
	int **erp = NULL;
	int ret = -1;
	unsigned a;

	if((erp = Malloc("ptrbuf",sizeof(*erp) * ALLOCATION_COUNT)) == NULL){
		return -1;
	}
	printf(" Constructing <int> slalloc...\n");
	if((sl = create_slalloc_int()) == NULL){
		goto done;
	}
	printf(" Allocating %d <int>s from slalloc...\n",ALLOCATION_COUNT);
	for(a = 0 ; a < ALLOCATION_COUNT ; ++a){
		if((erp[a] = slalloc_int_new(sl)) == NULL){
			goto done;
		}
		if(*erp[a]){
			fprintf(stderr," Got a dirty slalloc entry at %u\n",a);
			goto done;
		}
	}
	if(stringize_slalloc_int(&u,sl)){
		goto done;
	}
	printf(" %s\n",u.string);
	reset_ustring(&u);
	printf(" Deallocating %d <int>s from slalloc...\n",ALLOCATION_COUNT);
	for(a = 0 ; a < ALLOCATION_COUNT ; ++a){
		if(slalloc_int_free(sl,erp[a])){
			goto done;
		}
	}
#undef ALLOCATION_COUNT
	if(stringize_slalloc_int(&u,sl)){
		goto done;
	}
	printf(" %s\n",u.string);
	reset_ustring(&u);
	ret = 0;
	
done:
	printf(" Destroying <int> slalloc...\n");
	destroy_slalloc_int(sl);
	Free(erp);
	return ret;
}

#define ALLOCATION_COUNT 0x100000
#define slallocfollow(type) \
static int \
test_slalloc##type##follow(void){ \
	ustring u = USTRING_INITIALIZER; \
	struct slalloc_##type *sl; \
	int ret = -1; \
	unsigned a; \
	type *erp; \
\
	printf(" Constructing <"#type"> slalloc...\n"); \
	if((sl = create_slalloc_##type()) == NULL){ \
		return -1; \
	} \
	printf(" Allocating <"#type"> from slalloc %d times...\n",ALLOCATION_COUNT); \
	for(a = 0 ; a < ALLOCATION_COUNT ; ++a){ \
		if((erp = slalloc_##type##_new(sl)) == NULL){ \
			goto done; \
		} \
		if(slalloc_##type##_free(sl,erp)){ \
			goto done; \
		} \
	} \
	if(stringize_slalloc_##type(&u,sl)){ \
		goto done; \
	} \
	printf(" %s\n",u.string); \
	reset_ustring(&u); \
	ret = 0; \
	\
done: \
	printf(" Destroying <"#type"> slalloc...\n"); \
	destroy_slalloc_##type(sl); \
	return ret; \
}

slallocfollow(int)
slallocfollow(nastybuf)
slallocfollow(bigbuf)
// slallocfollow(obesebuf)
#undef ALLOCATION_COUNT

static int
test_slallocintfollowword(void){
	ustring u = USTRING_INITIALIZER;
	struct slalloc_int *sl;
	unsigned a,bpw;
	int ret = -1;

	bpw = sizeof(unsigned) * CHAR_BIT;
	printf(" Constructing <int> slalloc...\n");
	if((sl = create_slalloc_int()) == NULL){
		return -1;
	}
#define ALLOCATION_COUNT 0x100000
	printf(" Allocating %u <int> from slalloc %d times...\n",bpw,ALLOCATION_COUNT / bpw / 2);
	for(a = 0 ; a < ALLOCATION_COUNT / bpw / 2; ++a){
		int *erpbuf[bpw];
		unsigned b;

		for(b = 0 ; b < bpw ; ++b){
			if((erpbuf[b] = slalloc_int_new(sl)) == NULL){
				goto done;
			}
		}
		for(b = 0 ; b < bpw ; ++b){
			if(slalloc_int_free(sl,erpbuf[b])){
				goto done;
			}
		}
		for(b = 0 ; b < bpw ; ++b){
			if((erpbuf[b] = slalloc_int_new(sl)) == NULL){
				goto done;
			}
		}
		for(b = 0 ; b < bpw ; ++b){
			if(slalloc_int_free(sl,erpbuf[bpw - b - 1])){
				goto done;
			}
		}
	}
	if(stringize_slalloc_int(&u,sl)){
		goto done;
	}
	printf(" %s\n",u.string);
	reset_ustring(&u);
#undef ALLOCATION_COUNT
	ret = 0;
	
done:
	printf(" Destroying <int> slalloc...\n");
	destroy_slalloc_int(sl);
	return ret;
}

const declared_test SLALLOC_TESTS[] = {
	{	.name = "slallocnullkill",
		.testfxn = test_slallocnullkill,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "slallocspringup",
		.testfxn = test_slallocspringup,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "slallocshutitdown",
		.testfxn = test_slallocshutitdown,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "slallocloavesandints",
		.testfxn = test_slallocloavesandints,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "slallocinttracking",
		.testfxn = test_slallocinttracking,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
#define TYPEFOLLOW(type) \
	{	.name = "slalloc"#type"follow", \
		.testfxn = test_slalloc##type##follow, \
		.expected_result = EXIT_TESTSUCCESS, \
		.sec_required = 0, .mb_required = 0, .disabled = 0, \
	}
	TYPEFOLLOW(int),
	TYPEFOLLOW(nastybuf),
	TYPEFOLLOW(bigbuf),
	// TYPEFOLLOW(obesebuf),
	{	.name = "slallocintfollowword",
		.testfxn = test_slallocintfollowword,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
