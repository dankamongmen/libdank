#include <unistd.h>
#include <sys/signal.h>
#include <sys/ucontext.h>
#include <libdank/utils/string.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/tracing/threads.h>

typedef enum {
	THREAD_CREATED,			// init state, wait for thread's report
	THREAD_READY,			// thread initialized and is running
					//  (waits for confirmation, will clean)
	THREAD_FAILED,			// thread failed and exited
					//  (caller cleans)
	THREAD_CONFIRMED_LAUNCHED,	// thread confirmed, creator derefed
} traced_thread_state;

typedef struct stack_wrapper {
	logctx *lc;
	char *name;
	int detached;
	logged_tfxn tfxn;
	void *arg;
	pthread_cond_t statecond;
	pthread_mutex_t statelock;
	traced_thread_state state;
} stack_wrapper;

typedef struct thread_stack {
	stack_t ss;
	pthread_t tid;
	stack_wrapper *sw;
	struct thread_stack *next;
} thread_stack;

static thread_stack *thread_stacks;

void log_tstk(void *stack){
	thread_stack *ts;

	if(stack == NULL){
		nag("No stack information\n");
		return;
	}
	for(ts = thread_stacks ; ts ; ts = ts->next){
		if((char *)stack >= (char *)ts->ss.ss_sp && (char *)stack < (char *)ts->ss.ss_sp + ts->ss.ss_size){
			nag("Received on stack: %s\n",ts->sw->name);
			return;
		}
	}
	nag("Couldn't identify stack %p\n",stack);
}

static pthread_mutex_t sigstack_lock = PTHREAD_MUTEX_INITIALIZER;

static void
free_stack_wrapper(stack_wrapper **sw){
	if(sw && *sw){
		Pthread_mutex_destroy(&(*sw)->statelock);
		Free((*sw)->name);
		Free(*sw);
		*sw = NULL;
	}
}

static int
register_thread_stack(stack_t *ss,stack_wrapper *sw){
	thread_stack *ts;

	nag("Signals use %s %zub stack\n",ss->ss_flags & SS_DISABLE ?
			"default" : "alternate",ss->ss_size);
	if((ts = Malloc("thread stack entry",sizeof(*ts))) == NULL){
		return -1;
	}
	memset(ts,0,sizeof(*ts));
	memcpy(&ts->ss,ss,sizeof(ts->ss));
	ts->tid = pthread_self();
	ts->sw = sw;
	pthread_mutex_lock(&sigstack_lock);
		ts->next = thread_stacks;
		thread_stacks = ts;
	pthread_mutex_unlock(&sigstack_lock);
	return 0;
}

static int
remove_thread_stack(const char *name,pthread_t tid){
	thread_stack *ts,**parent;

	pthread_mutex_lock(&sigstack_lock);
	for(parent = &thread_stacks ; (ts = *parent) ; parent = &ts->next){
		if(pthread_equal(ts->tid,tid)){
			*parent = ts->next;
			break;
		}
	}
	pthread_mutex_unlock(&sigstack_lock);
	if(ts == NULL){
		nag("Couldn't find %s's sigstack\n",name);
		return -1;
	}
	free_stack_wrapper(&ts->sw);
	Free(ts);
	return 0;
}

int reap_traceable_thread(const char *name,pthread_t tid,cancel_helper bludgeon){
	int res = 0;

	Pthread_cancel(tid);
	if(bludgeon){
		nag("Waking up %s thread\n",name);
		bludgeon(tid);
		nag("Sent wakeup to %s thread\n",name);
	}
	res |= join_traceable_thread(name,tid);
	return res;
}
	
// the same, without cancellation or the function call
// FIXME let's eliminate the need to pass name by doing an early sigstack lookup
int join_traceable_thread(const char *name,pthread_t tid){
	int res = 0;
	void *ret;

	res |= Pthread_join(name,tid,&ret);
	// nag("Cleaning up sigstack\n");
	res |= remove_thread_stack(name,tid);
	return res;
}

static int
get_thread_stack(stack_t *ss){
	// pthread_attr_t attr;
	ucontext_t uctx;

	memset(ss,0,sizeof(*ss));
	// pthread_attr_getstack() and pthread_attr_getstackaddr() only work if
	// you used set_stack() or set_stackaddr() prior to creating the
	// thread...FIXME what, really? highly dubious --nlb
	/* if(pthread_attr_init(&attr)){
		moan("Couldn't initialize pthread_attr at %p\n",&attr);
		return -1;
	}
	if(pthread_attr_getstacksize(&attr,&ss->ss_size)){
		moan("Couldn't get stack size using %p\n",&attr);
		return -1;
	}
	if(pthread_attr_destroy(&attr)){
		moan("Couldn't destroy pthread_attr at %p\n",&attr);
		return -1;
	}
	// hack! sw is the first thing defined on thread stack
	ss->ss_sp = (void *)sw;
	nag("stack: %p size: %zu\n",ss->ss_sp,ss->ss_size); */
	// If SS_DISABLE, the values seem to always be 0/NULL, see above...FIXME
	if(Sigaltstack(NULL,ss) == 0 && !(ss->ss_flags & SS_DISABLE)){
		return 0;
	}
	if(getcontext(&uctx)){
		moan("Couldn't get user context\n");
		return -1;
	}
	*ss = uctx.uc_stack;
	ss->ss_flags |= SS_DISABLE;
	return 0;
}

static int
setup_sigstack(stack_wrapper *sw){
	stack_t ss;

	if(get_thread_stack(&ss)){
		return -1;
	}
	if(register_thread_stack(&ss,sw)){
		return -1;
	}
	return 0;
}

static void *
stack_wrapperfxn(void *void_stack_wrapper){
	stack_wrapper *sw = void_stack_wrapper;
	logctx *lc = sw->lc;

	if(sw->detached){
		init_detached_thread_logctx(lc,sw->name);
	}else{
		init_thread_logctx(lc,sw->name);
	}
	if(setup_sigstack(sw)){
		goto err;
	}
	pthread_mutex_lock(&sw->statelock);
	sw->state = THREAD_READY;
	pthread_cond_signal(&sw->statecond);
	pthread_mutex_unlock(&sw->statelock);

	sw->tfxn(sw->arg);

	// in case that exited really quickly, don't allow a race...
	goto done;

err:
	pthread_mutex_lock(&sw->statelock);
	sw->state = THREAD_FAILED;
	pthread_cond_signal(&sw->statecond);
	pthread_mutex_unlock(&sw->statelock);

done:
	nag("Traceable thread exiting\n");
	pthread_mutex_lock(&sw->statelock);
	while(sw->state != THREAD_CONFIRMED_LAUNCHED){
		nag("Waiting for launch confirmation, saw %d\n",sw->state);
		pthread_cond_wait(&sw->statecond,&sw->statelock);
	}
	pthread_mutex_unlock(&sw->statelock);
	if(sw->detached){
		nag("Detached thread cleaning up\n");
		if(remove_thread_stack(sw->name,pthread_self())){
			free_stack_wrapper(&sw);
		}
	}
	nag("Goodbye, cruel world!\n");
	return NULL;
}

static stack_wrapper *
create_stack_wrapper(const char *name,logged_tfxn tfxn,
					void *arg,int detached){
	stack_wrapper *ret;

	if((ret = Malloc("sigstack wrapper",sizeof(*ret))) == NULL){
		return NULL;
	}
	if((ret->lc = Malloc("logctx",sizeof(*ret->lc))) == NULL){
		Free(ret);
		return NULL;
	}
	if((ret->name = Strdup(name)) == NULL){
		Free(ret->lc);
		Free(ret);
		return NULL;
	}
	if(Pthread_mutex_init(&ret->statelock,NULL)){
		Free(ret->name);
		Free(ret->lc);
		Free(ret);
		return NULL;
	}
	if(Pthread_cond_init(&ret->statecond,NULL)){
		Pthread_mutex_destroy(&ret->statelock);
		Free(ret->name);
		Free(ret->lc);
		Free(ret);
		return NULL;
	}
	ret->detached = detached;
	ret->tfxn = tfxn;
	ret->arg = arg;
	ret->state = THREAD_CREATED;
	return ret;
}

// We need the name despite the copy in sw, because sw can be freed by the
// thread after we mark THREAD_CONFIRMED_LAUNCHED.
static int
confirm_traceable_thread(const char *name,stack_wrapper *sw){
	traced_thread_state state;

	pthread_mutex_lock(&sw->statelock);
	while((state = sw->state) == THREAD_CREATED){
		nag("Waiting for %s to confirm launch\n",name);
		pthread_cond_wait(&sw->statecond,&sw->statelock);
	}
	sw->state = THREAD_CONFIRMED_LAUNCHED;
	pthread_cond_signal(&sw->statecond);
	pthread_mutex_unlock(&sw->statelock);
	if(state == THREAD_READY){
		nag("Confirmed %s as running\n",name);
		return 0;
	}
	bitch("Created %s, but it failed immediately\n",name);
	return -1;
}

int new_traceable_detached(const char *name,logged_tfxn tfxn,void *arg){
	stack_wrapper *sw;
	pthread_attr_t pat;
	pthread_t tid;
	int ret;

	if(pthread_attr_init(&pat)){
		moan("Couldn't initialize pthread_attr_t for %s\n",name);
		return -1;
	}
	if(pthread_attr_setdetachstate(&pat,PTHREAD_CREATE_DETACHED)){
		moan("Couldn't initialize pthread_attr_t for %s\n",name);
		pthread_attr_destroy(&pat);
		return -1;
	}
	if((sw = create_stack_wrapper(name,tfxn,arg,1)) == NULL){
		pthread_attr_destroy(&pat);
		return -1;
	}
	if(Pthread_create(sw->name,&tid,&pat,stack_wrapperfxn,sw)){
		free_logctx(sw->lc);
		Free(sw->lc);
		free_stack_wrapper(&sw);
		pthread_attr_destroy(&pat);
		return -1;
	}
	nag("Detached %s thread\n",name);
	if( (ret = confirm_traceable_thread(name,sw)) ){
		free_stack_wrapper(&sw);
	}
	ret |= pthread_attr_destroy(&pat);
	return ret;
}

int new_traceable_thread(const char *name,pthread_t *tid,
					logged_tfxn tfxn,void *arg){
	stack_wrapper *sw;

	if(tid == NULL){
		bitch("Passed a NULL pthread_t *\n");
		return -1;
	}
	if((sw = create_stack_wrapper(name,tfxn,arg,0)) == NULL){
		return -1;
	}
	// Once pthread_create() returns success, we must never free the
	// sigstack wrapper. The spawned thread will handle it in all cases.
	if(Pthread_create(sw->name,tid,NULL,stack_wrapperfxn,sw)){
		free_logctx(sw->lc);
		Free(sw->lc);
		free_stack_wrapper(&sw);
		return -1;
	}
	nag("Created %s thread\n",name);
	if(confirm_traceable_thread(name,sw)){
		join_traceable_thread(name,*tid);
		free_stack_wrapper(&sw);
		*tid = 0;
		return -1;
	}
	return 0;
}
