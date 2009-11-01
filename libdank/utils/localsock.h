#ifndef UTILS_LOCALSOCK
#define UTILS_LOCALSOCK

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/un.h>

int connect_local(const char *);
int listen_local(const char *);

// Flags parameter is a union over the LIBDANK_FD_* values (see Accept4())
int accept_local(int,struct sockaddr_un *,int);

int connect_local_dgram(const char *);
int listen_local_dgram(const char *);

#ifdef __cplusplus
}
#endif

#endif
