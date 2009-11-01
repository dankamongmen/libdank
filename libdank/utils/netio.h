#ifndef LIBDANK_UTILS_NETIO
#define LIBDANK_UTILS_NETIO

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <arpa/inet.h>

struct in_addr;
struct in6_addr;
struct sockaddr;

// Decodes socket errors with getsockopt(), returning != 0 if there were any.
int log_socket_errors(int);
int make_nblock_tcp_socket(void);
int make_nlinger_tcp_socket(void);
int make_nblock_nlinger_tcp_socket(void);
int make_listener(const struct sockaddr *,socklen_t,int);
int make_listening_sd(const struct in6_addr *,unsigned,int);
int make_listening4_sd(const struct in_addr *,unsigned,int);
int read_socket_dynline(int,char **);
int is_sock_listening(int);
int get_socket_rcvbuf(int,int *);
int set_socket_rcvbuf(int,int);
int get_socket_sndbuf(int,int *);
int set_socket_sndbuf(int,int);

#ifdef __cplusplus
}
#endif

#endif
