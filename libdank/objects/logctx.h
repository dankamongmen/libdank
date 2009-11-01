#ifndef OBJECTS_LOGCTX
#define OBJECTS_LOGCTX

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <libdank/modules/logging/logging.h>

struct ustring;

// logctx's provide a general interface to logfiles, listeners, and ctlserver
// command feedback. There's one logctx per created thread, and one for
// the main thread.

#define LISTENER_MSG_SZ PIPE_BUF

typedef struct logctx {
	FILE *lfile;
	off_t lfile_offset;
	uintmax_t lineswritten;
	struct ustring *out,*err;
	char msg_buffer[LISTENER_MSG_SZ];
	char timebuf[27]; // see asctime_r(3)
	char lfile_name[PATH_MAX + 1];
	char strerrbuf[80]; // FIXME just a guess
	int cleanup;
} logctx;

void init_private_logctx(logctx *);
void init_thread_logctx(logctx *,const char *);
void init_detached_thread_logctx(logctx *,const char *);

int create_logctx_ustrings(void);
void free_logctx_ustrings(void);

void reset_logctx_ustrings(void);

// Get the current thread's logctx, if possible. This can fail due to internal
// limitations of the pthread library, logging not having been initialized, or
// the thread not having been properly created using the threading framework.
// In any such case, the return value will be NULL, and we fall back to stdio.
logctx *get_thread_logctx(void);

// Uses the logctx's strerrbuf, unlike strerror() which shares one global buf
const char *logctx_strerror_r(int);

// FIXME these functions ought suffix with \n, since they're logically line-
// based (given that they prefix with the function name). It'll be a big PITA
// going through every caller and killing the \n's already there, though.
#define nag(fmt,...) \
	flog("%s] "fmt,__func__ ,##__VA_ARGS__)

#define nagonbehalf(caller,fmt,...) \
	flog("%s] "fmt,caller ,##__VA_ARGS__)

#define timenag(fmt,...) \
	timeflog("%s] "fmt,__func__ ,##__VA_ARGS__)

#define bitch(fmt,...) \
	flog("***** Error in %s] "fmt,__func__ ,##__VA_ARGS__)

#define pmoan(errcode,fmt,...) \
	flog("***** Error (%s) in %s(): "fmt,logctx_strerror_r(errcode),\
			__func__ ,##__VA_ARGS__)

#define pmoanonbehalf(errcode,caller,fmt,...) \
	flog("***** Error (%s) in %s(): "fmt,logctx_strerror)r(errcode),\
			 caller ,##__VA_ARGS__)

// Preserves errno across flog()
#define moan(fmt,...) do{ \
	int m_err = errno; \
	pmoan(m_err,fmt ,##__VA_ARGS__); \
	errno = m_err; \
}while(0);

#define moanonbehalf(caller,fmt,...) do{ \
	int m_err = errno; \
	pmoanonbehalf(m_err,caller,fmt ,##__VA_ARGS__); \
	errno = m_err; \
}while(0);

void free_logctx(logctx *);

#ifdef __cplusplus
}
#endif

#endif
