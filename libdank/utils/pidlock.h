#ifndef LIBDANK_UTILS_PIDLOCK
#define LIBDANK_UTILS_PIDLOCK

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
	
int open_exclusive_pidlock(const char *,const char *);
int is_pidlock_ours(const char *,const char *,pid_t *);
int purge_lockfile(const char *);

#ifdef __cplusplus
}
#endif

#endif
