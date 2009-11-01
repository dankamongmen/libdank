#ifndef MODULES_LOGGING_LOGDIR
#define MODULES_LOGGING_LOGDIR

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

struct logctx;

int set_log_stdio(void);
int set_logdir(const char *);

// Supply the name of the thread, returns a logfile in the logdir. The filename is saved
// in fn, which must be at least PATH_MAX + 1 bytes long.
FILE *open_thread_log(const char *,char *);

// call in crash handler immediately
void get_crash_log(struct logctx *);

// call with the current lc at shutdown whether crash or not
void truncate_crash_log(int);

#ifdef __cplusplus
}
#endif

#endif
