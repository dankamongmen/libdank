#ifndef CROSIER_LIBCROSIER
#define CROSIER_LIBCROSIER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int crosier_connect(const char *,int);

int send_ctlrequest(int,const char *,FILE *);
int send_ctlrequest_buf(int,const char *,const char *);

int recv_ctlreply(int);

#ifdef __cplusplus
}
#endif

#endif
