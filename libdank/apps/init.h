#ifndef APPS_INIT_STDINIT
#define APPS_INIT_STDINIT

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <signal.h>
#include <libdank/apps/environ.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/logging/logging.h>

typedef struct app_def {
	envset *environ;
	const char *logdir;
	const char *appname;
	const char *confdir;
	const char *obnoxiousness;
	sigset_t blocksigs;
} app_def;

typedef struct app_ctx {
	sigset_t waitsigs;
	char *lockfile;
	const char *ctlsrvsocket;
} app_ctx;

int app_init_vercheck(const char *,logctx *,const app_def *,app_ctx *,int,
			char **,const char *,const char *);
int libdank_vercheck_internal(const char *);

int handle_fatal_sigs(void);
int handle_ignored_sigs(void);

// set during a successful app_init()
extern const app_def *application;

#ifdef __cplusplus
}
#endif

#endif
