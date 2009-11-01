#ifndef OBJECTS_CMDSTATE
#define OBJECTS_CMDSTATE

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

// both sd's are guaranteed to be valid, but not necessarily distinct.
typedef struct cmd_state {
	int sd,errsd;
	int sent_success; // used for log_dump
} cmd_state;

void init_cmd_state(cmd_state *,int,int);
int close_cmd_state(cmd_state *);

FILE *suck_socket_tmpfile(cmd_state *);
int dynsuck_socket_line(cmd_state *,char **);

int send_success(cmd_state *);

#ifdef __cplusplus
}
#endif

#endif
