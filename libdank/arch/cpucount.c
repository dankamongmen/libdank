#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/syswrap.h>
#include <libdank/arch/cpucount.h>
#include <libdank/utils/lineparser.h>
#include <libdank/modules/fileconf/sbox.h>

long num_cpus = 0;
static pthread_mutex_t cpu_count_lock = PTHREAD_MUTEX_INITIALIZER;

#if defined(_SC_NPROCESSORS_ONLN)
static long
parse_cpuinfo(void){
	return num_cpus = Sysconf(_SC_NPROCESSORS_ONLN);

}
#elif defined(CPUENUM_METHOD_PROC)
static const char CPUINFO[] = "/proc/cpuinfo";

static int
parse_proc_cpuinfo_line(char *buf,void *v __attribute__ ((unused))){
	int procnum;

	if(sscanf(buf," processor : %d",&procnum) != 1){
		return 0;
	}
	if(procnum != num_cpus && procnum){
		bitch("Malformed processor line: %d\n",procnum);
		return -1;
	}
	++num_cpus;
	return 0;
}

static int
parse_cpuinfo(void){
	int fd;

	if((fd = Open(CPUINFO,O_RDONLY)) < 0){
		return -1;
	}
	if(parser_byline(fd,parse_proc_cpuinfo_line,NULL)){
		num_cpus = 0;
		Close(fd);
		return -1;
	}
	if(Close(fd)){
		num_cpus = 0;
		return -1;
	}
	if(num_cpus == 0){
		return -1;
	}
	return 0;
}
#elif defined(CPUENUM_METHOD_SYSCTL)
static int
parse_cpuinfo(void){
	size_t ncpulen;
	int ncpu = -1;

	ncpulen = sizeof(ncpu);
	if(Sysctlbyname("hw.ncpu",&ncpu,&ncpulen,NULL,0)){
		return -1;
	}
	if(ncpu < 1){
		bitch("sysctlbyname(hw.ncpu) returned %d\n",ncpu);
		return -1;
	}
	num_cpus = ncpu;
	return 0;
}
#else
static int
parse_cpuinfo(void){
	nag("Processor detection not supported on this OS\n");
	num_cpus = 1; // I'm pretty sure we've gotta have at least one.
	return 0;
}
#endif

long detect_num_processors(void){
	long ret;

	pthread_mutex_lock(&cpu_count_lock);
	if(num_cpus == 0){
		parse_cpuinfo();
		nag("Detected %ld CPU%s\n",num_cpus,num_cpus == 1 ? "" : "'s");
	}
	ret = num_cpus;
	pthread_mutex_unlock(&cpu_count_lock);
	return ret;
}

static int
null_lock_fxn(pthread_mutex_t *lock __attribute__ ((unused))){
	return 0;
}

int initialize_cpu_based_lock(cpu_based_lock *cbl){
	long num;

	if((num = detect_num_processors()) <= 0){
		return -1;
	}
	if(pthread_mutex_init(&cbl->lock,NULL)){
		return -1;
	}
	if(num == 1){
		cbl->lockfxn = null_lock_fxn;
		cbl->unlockfxn = null_lock_fxn;
	}else{
		cbl->lockfxn = pthread_mutex_lock;
		cbl->lockfxn = pthread_mutex_unlock;
	}
	return 0;
}

int destroy_cpu_based_lock(cpu_based_lock *cbl){
	if(pthread_mutex_destroy(&cbl->lock)){
		return -1;
	}
	return 0;
}
