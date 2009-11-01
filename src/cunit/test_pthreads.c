#include <unistd.h>
#include <cunit/cunit.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/tracing/threads.h>

// Do not statically initialize using PTHREAD_MUTEX_INITIALIZER; that's
// another test from this sequence (test_mutex_destroystatic).
static pthread_mutex_t destroy_dyn_static_lock;

static int
test_mutex_destroyinit(void){
	pthread_mutex_t lock,*heaplock;
	int ret = -1;

	if((heaplock = Malloc("heaplock",sizeof(*heaplock))) == NULL){
		goto done;
	}
	if(Pthread_mutex_init(heaplock,NULL)){
		goto done;
	}
	if(Pthread_mutex_init(&lock,NULL)){
		goto done;
	}
	if(Pthread_mutex_init(&destroy_dyn_static_lock,NULL)){
		goto done;
	}
	printf(" Destroying dynamic-init static lock...\n");
	if(Pthread_mutex_destroy(&destroy_dyn_static_lock)){
		goto done;
	}
	printf(" Destroying dynamic-init stack lock...\n");
	if(Pthread_mutex_destroy(&lock)){
		goto done;
	}
	printf(" Destroying dynamic-init heap lock...\n");
	if(Pthread_mutex_destroy(heaplock)){
		goto done;
	}
	ret = 0;

done:
	Free(heaplock);
	return ret;
}

static pthread_mutex_t destroy_static_lock = PTHREAD_MUTEX_INITIALIZER;

static int
test_mutex_destroystatic(void){
	int ret = -1;

	printf(" Destroying static-init lock...\n");
	pthread_mutex_lock(&destroy_static_lock);
	pthread_mutex_unlock(&destroy_static_lock);
	ret = Pthread_mutex_destroy(&destroy_static_lock);
	return ret;
}

static int unfair;
static pthread_cond_t fair_cond;
static pthread_mutex_t fair_lock;

static void
iobound_main(void *nil __attribute__ ((unused))){
	int seconds = 0;

	for( ; ; ){
		printf(" Locking from bound_main...\n");
		PTHREAD_LOCK(&fair_lock);
		sleep(1);
		PTHREAD_UNLOCK(&fair_lock);
		pthread_testcancel();
		++seconds;
		printf(" Slept %d second%s...\n",seconds,seconds == 1 ? "" : "s");
	}
	bitch("This should never be reached!\n");
}

static void
iobound_yield_main(void *nil __attribute__ ((unused))){
	int seconds = 0;

	for( ; ; ){
		printf(" Locking from yield_main...\n");
		PTHREAD_LOCK(&fair_lock);
		sleep(1);
		PTHREAD_UNLOCK(&fair_lock);
		sleep(1);
		seconds += 2;
		printf(" Slept %d second%s...\n",seconds,seconds == 1 ? "" : "s");
	}
	bitch("This should never be reached!\n");
}

static void
helper_main(void *nil __attribute__ ((unused))){
	printf(" Trying to take lock from sleeping thread...\n");
	pthread_mutex_lock(&fair_lock);
	unfair = 0;
	pthread_cond_signal(&fair_cond);
	pthread_mutex_unlock(&fair_lock);
	printf(" Signaled unit test condition variable.\n");
	nag("Helper thread exiting!\n");
}

static int
test_iobound_fairness(void){
	pthread_t iotid,helptid;
	int ret = -1;

	unfair = 1;
	if(Pthread_mutex_init(&fair_lock,NULL)){
		return -1;
	}
	if(Pthread_cond_init(&fair_cond,NULL)){
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	if(new_traceable_thread("iobound",&iotid,iobound_main,NULL)){
		Pthread_cond_destroy(&fair_cond);
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	if(new_traceable_thread("helper",&helptid,helper_main,NULL)){
		reap_traceable_thread("iobound",iotid,NULL);
		Pthread_cond_destroy(&fair_cond);
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	pthread_mutex_lock(&fair_lock);
	while(unfair){
		pthread_cond_wait(&fair_cond,&fair_lock);
	}
	pthread_mutex_unlock(&fair_lock);
	ret = 0;
	ret |= join_traceable_thread("helper",helptid);
	ret |= reap_traceable_thread("iobound",iotid,NULL);
	ret |= Pthread_cond_destroy(&fair_cond);
	ret |= Pthread_mutex_destroy(&fair_lock);
	return ret;
}

static int
test_iobound_yielding(void){
	pthread_t iotid,helptid;
	int ret = -1;

	unfair = 1;
	if(Pthread_mutex_init(&fair_lock,NULL)){
		return -1;
	}
	if(Pthread_cond_init(&fair_cond,NULL)){
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	if(new_traceable_thread("iobound_yielding",&iotid,iobound_yield_main,NULL)){
		Pthread_cond_destroy(&fair_cond);
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	if(new_traceable_thread("helper",&helptid,helper_main,NULL)){
		reap_traceable_thread("iobound_yielding",iotid,NULL);
		Pthread_cond_destroy(&fair_cond);
		Pthread_mutex_destroy(&fair_lock);
		return -1;
	}
	pthread_mutex_lock(&fair_lock);
	while(unfair){
		printf(" Blocking on yield...\n");
		pthread_cond_wait(&fair_cond,&fair_lock);
	}
	pthread_mutex_unlock(&fair_lock);
	ret = 0;
	ret |= join_traceable_thread("helper",helptid);
	ret |= reap_traceable_thread("iobound_yielding",iotid,NULL);
	ret |= Pthread_cond_destroy(&fair_cond);
	ret |= Pthread_mutex_destroy(&fair_lock);
	return ret;
}

const declared_test PTHREADS_TESTS[] = {
	{	.name = "mutex_destroyinit",
		.testfxn = test_mutex_destroyinit,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "mutex_destroystatic",
		.testfxn = test_mutex_destroystatic,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "iobound_fairness",
		.testfxn = test_iobound_fairness,
		.expected_result = EXIT_TESTSUCCESS | EXIT_SIGNAL,
		.sec_required = 0, .mb_required = 0, .disabled = 1,
	},
	{	.name = "iobound_yielding",
		.testfxn = test_iobound_yielding,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
