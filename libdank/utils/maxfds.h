#ifndef UTILS_MAXFDS
#define UTILS_MAXFDS

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <libdank/objects/logctx.h>

int determine_max_fds(void);

void *allocate_per_possible_fd(const char *,size_t)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

// none of these will return an sd higher than max_sd, performing sanity checks
int safe_socket(int,int,int);

// return -1 if fd is > max_fds, fd otherwise
int sanity_check_fd(int);

#ifdef __cplusplus
}
#endif

#endif
