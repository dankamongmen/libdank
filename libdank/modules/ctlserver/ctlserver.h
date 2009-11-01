#ifndef MODULES_CTLSERVER_CTLSERVER
#define MODULES_CTLSERVER_CTLSERVER

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/objects/cmdstate.h>

#include <pthread.h>

int init_ctlserver(const char *);
int stop_ctlserver(void);

typedef int (*ctlserv_handler)(cmd_state *);

typedef struct command {
	const char *cmd;
	ctlserv_handler func;
} command;

int regcommands(const command *);
int delcommands(const command *);

struct ustring;

typedef int (*stringizer)(struct ustring *);

int dump_lock(stringizer,pthread_mutex_t *);

int dump(stringizer);

#ifdef __cplusplus
}
#endif

#endif
