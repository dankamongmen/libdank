#ifndef MODULES_LOGGING_LOGGING
#define MODULES_LOGGING_LOGGING

#ifdef __cplusplus
extern "C" {
#endif

struct logctx;

int init_logging(struct logctx *,const char *,int);
int stop_logging(int);

// These are all based off per-thread logging (we perform a lookup on thread-
// specific data, and log to that object).
void vflog(const char *,va_list);
void flog(const char *,...) __attribute__ ((format (printf,1,2)));
void timeflog(const char *,...) __attribute__ ((format (printf,1,2)));

// These allow logging to a specified file.

void log_crash(struct logctx *);

int init_log_server(void);
int stop_log_server(void);

#ifdef __cplusplus
}
#endif

#endif
