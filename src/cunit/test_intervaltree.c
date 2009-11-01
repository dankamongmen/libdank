#include <stdlib.h>
#include <cunit/cunit.h>
#include <libdank/utils/string.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/intervaltree.h>

static int
test_intervaltree_freeempty(void){
	struct interval_tree *it = NULL;

	free_interval_tree(&it,NULL);
	return 0;
}

static struct treenode {
	const interval ival;
	const char *data;
} treenodes[] = {
	{	.ival = { .lbound = 0,	.ubound = 0,	},
		.data = "lbound",
	},{
		.ival = { .lbound = 0xffffffff,	.ubound = 0xffffffff,	},
		.data = "ubound",
	},{
		.ival = { .lbound = 2,	.ubound = 0xfffffffd,	},
		.data = "fullopen",
	},{
		.ival = { .lbound = 0xfffffffe,	.ubound = 0xfffffffe,	},
		.data = "rinsert",
	},{
		.ival = { .lbound = 1,	.ubound = 1,	},
		.data = "linsert",
	},{
		.ival = { .lbound = 0,	.ubound = 0xffffffff,	},
		.data = NULL,
	}
};

static void
Free_node(void *v){
	Free(v);
}

static int
test_intervaltree32(void){
	struct interval_tree *it = NULL;
	typeof(*treenodes) *tn;
	int ret = -1;

	for(tn = treenodes ; tn->data ; ++tn){
		char *cpy;

		if((cpy = Strdup(tn->data)) == NULL){
			goto done;
		}
		printf(" Adding %s: [%u:%u].\n",tn->data,tn->ival.lbound,tn->ival.ubound);
		if(insert_interval_tree(&it,&tn->ival,cpy)){
			Free(cpy);
			goto done;
		}
	}
	for(tn = treenodes ; tn->data ; ++tn){
		void *v;

		printf(" Looking up %u: ",tn->ival.lbound);
		if((v = lookup_interval_tree(it,tn->ival.lbound)) == NULL){
			goto done;
		}
		if(strcmp(v,tn->data)){
			goto done;
		}
		printf("%s, success.\n",tn->data);
		if(tn->ival.ubound == tn->ival.lbound){
			continue;
		}
		printf(" Looking up %u: ",tn->ival.ubound);
		if((v = lookup_interval_tree(it,tn->ival.ubound)) == NULL){
			printf("lookup failure.\n");
			goto done;
		}
		if(strcmp(v,tn->data)){
			printf("%s, failure.\n",(char *)v);
			goto done;
		}
		printf("%s, success.\n",tn->data);
	}
	printf(" Tree depth: %u.\n",depth_interval_tree(it));
	printf(" Tree population: %u.\n",population_interval_tree(it));
	if(population_interval_tree(it) != (unsigned)(tn - treenodes)){
		printf(" Population mismatch; should have been %td\n",tn - treenodes);
		goto done;
	}
	balance_interval_tree(&it);
	printf(" Tree depth: %u.\n",depth_interval_tree(it));
	#define REPLACESUFFIX " (replaced)"
	for(tn = treenodes ; tn->data ; ++tn){
		char *cpy;

		if((cpy = Malloc("replacestr",strlen(tn->data) + strlen(REPLACESUFFIX) + 1)) == NULL){
			goto done;
		}
		strcpy(cpy,tn->data);
		strcat(cpy,REPLACESUFFIX);
		printf(" Replacing %s with %s: [%u:%u].\n",tn->data,cpy,tn->ival.lbound,tn->ival.ubound);
		if(replace_interval_tree(&it,&tn->ival,cpy,Free_node)){
			Free(cpy);
			goto done;
		}
	}
	for(tn = treenodes ; tn->data ; ++tn){
		void *v;

		printf(" Looking up %u: ",tn->ival.lbound);
		if((v = lookup_interval_tree(it,tn->ival.lbound)) == NULL){
			printf("lookup failure.\n");
			goto done;
		}
		if(strncmp(v,tn->data,strlen(tn->data))){
			printf("%s, failure.\n",(char *)v);
			goto done;
		}
		if(strcmp((char *)v + strlen(tn->data),REPLACESUFFIX)){
			printf("%s, failure.\n",(char *)v);
			goto done;
		}
		printf("%s, success.\n",(char *)v);
		if(tn->ival.ubound == tn->ival.lbound){
			continue;
		}
		printf(" Looking up %u: ",tn->ival.ubound);
		if((v = lookup_interval_tree(it,tn->ival.ubound)) == NULL){
			printf("lookup failure.\n");
			goto done;
		}
		if(strncmp(v,tn->data,strlen(tn->data))){
			printf("%s, failure.\n",(char *)v);
			goto done;
		}
		if(strcmp((char *)v + strlen(tn->data),REPLACESUFFIX)){
			printf("%s, failure.\n",(char *)v);
			goto done;
		}
		printf("%s, success.\n",(char *)v);
	}
	#undef REPLACESUFFIX
	for(tn = treenodes ; tn->data ; ++tn){
		printf(" Removing [%u:%u]: ",tn->ival.lbound,tn->ival.ubound);
		if(remove_interval_tree(&it,&tn->ival,Free_node)){
			printf(" removal failure.\n");
			goto done;
		}
		printf("success.\n");
		printf(" Looking up %u: ",tn->ival.lbound);
		if(lookup_interval_tree(it,tn->ival.lbound)){
			printf("lookup succeeded.\n");
			goto done;
		}
		printf("no data, success.\n");
		if(tn->ival.ubound == tn->ival.lbound){
			continue;
		}
		printf(" Looking up %u: ",tn->ival.ubound);
		if(lookup_interval_tree(it,tn->ival.ubound)){
			printf("lookup succeeded.\n");
			goto done;
		}
		printf("no data, success.\n");
	}
	ret = 0;

done:
	free_interval_tree(&it,Free_node);
	if(it){
		return -1;
	}
	return ret;
}

#define NODETARGET 0xfff
static int
test_intervaltree_longandhard(void){
	struct interval_tree *it = NULL;
	unsigned depth;
	uint32_t val;
	int ret = -1;

	printf(" Building a pointwise-dense interval tree...");
	fflush(stdout);
	for(val = 0 ; val < NODETARGET ; ++val){
		interval ival = { .lbound = val, .ubound = val, };

		if(insert_interval_tree(&it,&ival,NULL)){
			printf("failure after %u.\n",val);
			goto done;
		}
	}
	printf("success.\n");
	depth = depth_interval_tree(it);
	printf(" Tree depth: %u; balancing...",depth);
	fflush(stdout);
	balance_interval_tree(&it);
	depth = depth_interval_tree(it);
	printf(" depth: %u.\n",depth);
	fflush(stdout);
	ret = 0;

done:
	free_interval_tree(&it,Free_node);
	return ret;
}

static int
test_intervaltree_manofsteel(void){
	struct interval_tree *it = NULL;
	unsigned depth;
	uint32_t val;
	int ret = -1;

	printf(" Building an interval-dense interval tree...");
	fflush(stdout);
	for(val = 0 ; val < NODETARGET ; ++val){
		interval ival = { .lbound = 2 * val, .ubound = 2 * val + 1, };

		if(insert_interval_tree(&it,&ival,NULL)){
			printf("failure after %u.\n",val);
			goto done;
		}
	}
	printf("success.\n");
	depth = depth_interval_tree(it);
	printf(" Tree depth: %u; balancing...",depth);
	fflush(stdout);
	balance_interval_tree(&it);
	depth = depth_interval_tree(it);
	printf(" depth: %u.\n",depth);
	fflush(stdout);
	ret = 0;

done:
	free_interval_tree(&it,Free_node);
	return ret;
}

static int
test_intervaltree_skippinggayly(void){
	struct interval_tree *it = NULL;
	unsigned depth;
	uint32_t val;
	int ret = -1;

	printf(" Building a gap-dense interval tree...");
	fflush(stdout);
	for(val = 0 ; val < NODETARGET ; ++val){
		interval ival = { .lbound = 3 * val, .ubound = 3 * val + 1, };

		if(insert_interval_tree(&it,&ival,NULL)){
			printf("failure after %u.\n",val);
			goto done;
		}
	}
	printf("success.\n");
	depth = depth_interval_tree(it);
	printf(" Tree depth: %u; balancing...",depth);
	fflush(stdout);
	balance_interval_tree(&it);
	depth = depth_interval_tree(it);
	printf(" depth: %u.\n",depth);
	fflush(stdout);
	ret = 0;

done:
	free_interval_tree(&it,Free_node);
	return ret;
}

static int
test_intervaltree_deranged(void){
	struct interval_tree *it = NULL;
	void *sentinel = NULL;
	unsigned depth;
	uint32_t val;
	int ret = -1;

	printf(" Building an interval-sparse interval tree...");
	fflush(stdout);
	for(val = 0 ; val < NODETARGET ; ++val){
		interval ival;
		
		do{
			ival.lbound = (rand() % 0xfffe) * 2;
		}while(lookup_interval_tree(it,ival.lbound));
		ival.ubound = ival.lbound + 1;
		if(insert_interval_tree(&it,&ival,&sentinel)){
			printf("failure after %u.\n",val);
			goto done;
		}
	}
	printf("success.\n");
	depth = depth_interval_tree(it);
	printf(" Tree depth: %u; balancing...",depth);
	fflush(stdout);
	balance_interval_tree(&it);
	depth = depth_interval_tree(it);
	printf(" depth: %u.\n",depth);
	fflush(stdout);
	ret = 0;

done:
	free_interval_tree(&it,NULL);
	return ret;
}
#undef NODETARGET

const declared_test INTERVAL_TREE_TESTS[] = {
	{	.name = "intervaltree_freeempty",
		.testfxn = test_intervaltree_freeempty,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "intervaltree32",
		.testfxn = test_intervaltree32,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "intervaltree_longandhard",
		.testfxn = test_intervaltree_longandhard,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "intervaltree_manofsteel",
		.testfxn = test_intervaltree_manofsteel,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "intervaltree_skippinggayly",
		.testfxn = test_intervaltree_skippinggayly,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "intervaltree_deranged",
		.testfxn = test_intervaltree_deranged,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
