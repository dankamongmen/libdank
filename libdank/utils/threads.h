#ifndef UTILS_THREADS
#define UTILS_THREADS

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <libdank/objects/logctx.h>

// FIXME we ought track initialized locks vs destroyed locks, to determine
// leaks (ala memlimit.c). we might also do the same with threads themselves.

void cleanup_sem(void *);
void cleanup_mutex(void *);

typedef void(*cancel_helper)(pthread_t);

int _Pthread_mutex_init(const char *,pthread_mutex_t *,const pthread_mutexattr_t *);
int _Pthread_mutex_destroy(const char *,pthread_mutex_t *);
int _Pthread_cond_destroy(const char *,pthread_cond_t *);
int _Pthread_cond_init(const char *,pthread_cond_t *,const pthread_condattr_t *);

int Pthread_create(const char *,pthread_t *,pthread_attr_t *,
			void *(*)(void *),void *);
int Pthread_cancel(pthread_t);
int Pthread_kill(pthread_t,int);
int Pthread_join(const char *,pthread_t,void **);
int Pthread_sigmask(int,const sigset_t *,sigset_t *);

int Sem_init(const char *,sem_t *,unsigned);
int Sem_destroy(const char *,sem_t *);

#define PTHREAD_LOCK(lock) do { \
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL); \
	pthread_mutex_lock(lock); } while(0)

#define PTHREAD_UNLOCK(lock) do { \
	pthread_mutex_unlock(lock); \
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); } while(0)

#define PTHREAD_PUSH(lock,fn) \
	pthread_cleanup_push(fn,lock); \
	pthread_mutex_lock(lock)

#define PTHREAD_POP() pthread_cleanup_pop(1)

#define Pthread_mutex_init(mutex,mutexattr) _Pthread_mutex_init(__func__,mutex,mutexattr)
#define Pthread_mutex_destroy(mutex) _Pthread_mutex_destroy(__func__,mutex)

static inline int
_Pthread_mutex_lock(const char *func,pthread_mutex_t *lock){
	int ret;

	if((ret = pthread_mutex_lock(lock)) == 0){
		return 0;
	}
	pmoanonbehalf(ret,func,"Couldn't lock mutex\n");
	return -1;
}

static inline int
_Pthread_mutex_unlock(const char *func,pthread_mutex_t *lock){
	int ret;

	if((ret = pthread_mutex_unlock(lock)) == 0){
		return 0;
	}
	pmoanonbehalf(ret,func,"Couldn't unlock mutex\n");
	return -1;
}

#define Pthread_mutex_lock(mutex) _Pthread_mutex_lock(__func__,mutex)
#define Pthread_mutex_unlock(mutex) _Pthread_mutex_unlock(__func__,mutex)

#define Pthread_cond_init(cond,condattr) _Pthread_cond_init(__func__,cond,condattr)

#define Pthread_cond_destroy(cond) _Pthread_cond_destroy(__func__,cond)

static inline int
_Pthread_once(const char *func,pthread_once_t *once,void (*initfxn)(void)){
	int ret;

	if((ret = pthread_once(once,initfxn)) == 0){
		return 0;
	}
	pmoanonbehalf(ret,func,"Couldn't perform one-time initialization\n");
	return -1;
}

#define Pthread_once(once,initfxn) _Pthread_once(__func__,once,initfxn)

#ifdef __cplusplus
}
#endif

#endif
