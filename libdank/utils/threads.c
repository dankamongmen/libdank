#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/syswrap.h>

void cleanup_mutex(void *mutex){
	pthread_mutex_unlock(mutex);
}

void cleanup_sem(void *sem){
	sem_post(sem);
}

int Pthread_cancel(pthread_t tid){
	int ret;

	if( (ret = pthread_cancel(tid)) ){
		pmoan(ret,"Error sending POSIX cancellation\n");
		return -1;
	}
	nag("Sent POSIX cancellation\n");
	return 0;
}

int Pthread_kill(pthread_t tid,int sig){
	int ret;

	if( (ret = pthread_kill(tid,sig)) ){
		pmoan(ret,"Error sending %s to thread\n",strsignal(sig));
	}
	return ret;
}

int Pthread_create(const char *name,pthread_t *tid,
			pthread_attr_t * attr,void *(*fxn)(void *),void *arg){
	pthread_t basetid;
	int ret;

	if(name == NULL){
		bitch("Didn't provide a name for the thread\n");
		return -1;
	}
	if(fxn == NULL){
		bitch("Didn't provide a function for the thread\n");
		return -1;
	}
	if(tid == NULL){
		tid = &basetid;
	}
	timenag("Creating thread: %s\n",name);
	if( (ret = pthread_create(tid,attr,fxn,arg)) ){
		pmoan(ret,"Couldn't create %s thread\n",name);
		return -1;
	}
	return 0;
}

int Pthread_join(const char *name,pthread_t tid,void **thread_return){
	int ret;

	nag("Joining %s thread\n",name);
	if( (ret = pthread_join(tid,thread_return)) ){
		pmoan(ret,"Couldn't join %s thread\n",name);
		return -1;
	}
	nag("Joined %s; resources deallocated\n",name);
	return 0;
}

int Pthread_sigmask(int how,const sigset_t *ss,sigset_t *oss){
	int ret;

	if( (ret = pthread_sigmask(how,ss,oss)) ){
		pmoan(ret,"Couldn't set sigmask\n");
		return -1;
	}
	return 0;
}

int _Pthread_cond_init(const char *func,pthread_cond_t *cond,const pthread_condattr_t *condattr){
	int ret;

	if( (ret = pthread_cond_init(cond,condattr)) ){
		pmoanonbehalf(ret,func,"Couldn't initialize condvar\n");
		return -1;
	}
	return 0;
}

int _Pthread_cond_destroy(const char *func,pthread_cond_t *cond){
	int ret;

	nagonbehalf(func,"Destroying condvar\n");
	if( (ret = pthread_cond_destroy(cond)) ){
		pmoanonbehalf(ret,func,"Couldn't destroy condvar\n");
		return -1;
	}
	return 0;
}

int _Pthread_mutex_init(const char *func,pthread_mutex_t *lock,const pthread_mutexattr_t *mutexattr){
	int ret;

	if( (ret = pthread_mutex_init(lock,mutexattr)) ){
		pmoanonbehalf(ret,func,"Couldn't initialize mutex\n");
		return -1;
	}
	return 0;
}

int _Pthread_mutex_destroy(const char *func,pthread_mutex_t *lock){
	int ret;

	nagonbehalf(func,"Destroying mutex\n");
	if( (ret = pthread_mutex_destroy(lock)) ){
		pmoanonbehalf(ret,func,"Couldn't destroy mutex\n");
		return -1;
	}
	return 0;
}

int Sem_init(const char *name,sem_t *sem,unsigned value){
	if(sem_init(sem,0,value)){
		moan("Couldn't initialize %s semaphore\n",name);
		return -1;
	}
	return 0;
}

int Sem_destroy(const char *name,sem_t *sem){
	nag("Destroying %s semaphore\n",name);
	if(sem_destroy(sem)){
		moan("Couldn't destroy %s semaphore\n",name);
		return -1;
	}
	nag("Cleanly destroyed %s semaphore\n",name);
	return 0;
}
