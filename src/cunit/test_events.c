#include <signal.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <cunit/cunit.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/syswrap.h>
#include <libdank/modules/events/fds.h>
#include <libdank/modules/events/evcore.h>
#include <libdank/modules/events/signals.h>

static int
test_evhandler(void){
	int ret = -1;
	evhandler *e;

	if((e = create_evhandler(0)) == NULL){
		goto done;
	}
	if(fd_cloexecp(e->fd)){
		fprintf(stderr," Event queue's FD_CLOEXEC flag was set.\n");
		goto done;
	}
	printf(" Verified event queue's FD_CLOEXEC flag was unset.\n");
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	return ret;
}

static int
test_evhandler_cloexec(void){
	int ret = -1;
	evhandler *e;

	if((e = create_evhandler(LIBDANK_FD_CLOEXEC)) == NULL){
		goto done;
	}
	if(!fd_cloexecp(e->fd)){
		fprintf(stderr," Event queue's FD_CLOEXEC flag was unset.\n");
		goto done;
	}
	printf(" Verified event queue's FD_CLOEXEC flag was set.\n");
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	return ret;
}

static int
test_evhandler_badflags(void){
	int ret = -1;
	evhandler *e;

	if( (e = create_evhandler(~0)) ){
		fprintf(stderr," Got a valid event handler despite bad flags.\n");
		goto done;
	}
	printf(" Verified reject of bad flags to create_evhandler().\n");
	// also tests that we can call destroy_evhandler() on a NULL evhandler
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	return ret;
}

static int
test_evpethreads(unsigned tcount){
	evhandler *e;
	int ret = -1;
	unsigned n;

	if((e = create_evhandler(LIBDANK_FD_CLOEXEC)) == NULL){
		goto done;
	}
	for(n = 0 ; n < tcount ; ++n){
		if(spawn_evthread(e)){
			goto done;
		}
	}
	printf(" Spawned %u evhandler thread%s.\n",tcount,tcount == 1 ? "" : "s");
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	return 0;
}

static int
test_evpesingleton(void){
	return test_evpethreads(1);
}

static int
test_evpeherd(void){
	return test_evpethreads(32);
}

struct event_wrapper {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int sem;
};

static void
event_handler(int s,void *v){
	struct event_wrapper *ew = v;

	if(pthread_mutex_lock(&ew->lock) == 0){
		nag("Got identifier %d\n",s);
		++ew->sem;
		pthread_cond_signal(&ew->cond);
		pthread_mutex_unlock(&ew->lock);
	}
}

static int
block_on_event(pthread_mutex_t *lock,pthread_cond_t *cond,int *sem,int expected){
	if(pthread_mutex_lock(lock)){
		return -1;
	}
	while(*sem != expected){
		printf(" Got %d, waiting on %d.\n",*sem,expected);
		if(pthread_cond_wait(cond,lock)){
			return -1;
		}
	}
	if(pthread_mutex_unlock(lock)){
		return -1;
	}
	return 0;
}

static int
test_evsignal(void){
	struct event_wrapper ew = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
		.sem = 0,
	};
	evhandler *e = NULL;
	int mins = SIGSTOP + 1;
	int maxs = mins + 10; // FIXME
	int ret = -1,s;

	if((e = create_evhandler(LIBDANK_FD_CLOEXEC)) == NULL){
		goto done;
	}
	for(s = mins ; s <= maxs ; ++s){
		if(add_signal_to_evhandler(e,s,event_handler,&ew)){
			goto done;
		}
	}
	if(spawn_evthread(e)){
		goto done;
	}
	for(s = mins ; s <= maxs ; ++s){
		if(kill(getpid(),s)){
			goto done;
		}
	}
	if(block_on_event(&ew.lock,&ew.cond,&ew.sem,maxs - mins + 1)){
		goto done;
	}
	printf(" Validated %d signals.\n",maxs - mins + 1);
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	ret |= Pthread_cond_destroy(&ew.cond);
	ret |= Pthread_mutex_destroy(&ew.lock);
	return 0;
}

#define EVTEST_PORT 40000

static int
test_evtcpaccept(void){
	struct event_wrapper ew = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
		.sem = 0,
	};
	int ret = -1,fd = -1,cfd = -1;
	evhandler *e = NULL;
	struct sockaddr_in sa;

	if((e = create_evhandler(LIBDANK_FD_CLOEXEC)) == NULL){
		goto done;
	}
	memset(&sa,0,sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(EVTEST_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if((fd = make_listening4_sd(&sa.sin_addr,EVTEST_PORT,SOMAXCONN)) < 0){
		goto done;
	}
	printf(" Adding sd %d to the event queue.\n",fd);
	if(add_fd_to_evhandler(e,fd,event_handler,NULL,&ew)){
		goto done;
	}
	if(spawn_evthread(e)){
		goto done;
	}
	if((cfd = Socket(PF_INET,SOCK_STREAM,0)) < 0){
		goto done;
	}
	if(Connect(cfd,(const struct sockaddr *)&sa,sizeof(sa))){
		goto done;
	}
	if(block_on_event(&ew.lock,&ew.cond,&ew.sem,1)){
		goto done;
	}
	printf(" Validated TCP accept(2).\n");
	ret = 0;

done:
	ret |= destroy_evhandler(e);
	ret |= Pthread_cond_destroy(&ew.cond);
	ret |= Pthread_mutex_destroy(&ew.lock);
	if(fd >= 0){
		ret |= Close(fd);
	}
	if(cfd >= 0){
		ret |= Close(cfd);
	}
	return 0;
}

const declared_test EVENT_TESTS[] = {
	{	.name = "evhandler",
		.testfxn = test_evhandler,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "evhandler_cloexec",
		.testfxn = test_evhandler_cloexec,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "evhandler_badflags",
		.testfxn = test_evhandler_badflags,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "evpesingleton",
		.testfxn = test_evpesingleton,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "evpeherd",
		.testfxn = test_evpeherd,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 128, .disabled = 0,
	},
	{	.name = "evsignal",
		.testfxn = test_evsignal,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = "evtcpaccept",
		.testfxn = test_evtcpaccept,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
