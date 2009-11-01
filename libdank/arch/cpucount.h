#ifndef ARCH_CPUCOUNT
#define ARCH_CPUCOUNT

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

typedef int (*cpu_based_lock_fxn)(pthread_mutex_t *);

typedef struct cpu_based_lock {
	pthread_mutex_t lock;
	cpu_based_lock_fxn lockfxn;
	cpu_based_lock_fxn unlockfxn;
} cpu_based_lock;

// this must be called, and must return > 0, before num_cpus
// below should be trusted
long detect_num_processors(void);

extern long num_cpus;

int initialize_cpu_based_lock(cpu_based_lock *);
int destroy_cpu_based_lock(cpu_based_lock *);

#ifdef __cplusplus
}
#endif

#endif
