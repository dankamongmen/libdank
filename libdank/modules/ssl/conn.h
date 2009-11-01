#ifndef LIBDANK_MODULES_SSL_CONN
#define LIBDANK_MODULES_SSL_CONN

#ifdef __cplusplus
extern "C" {
#endif

struct ssl_server;
struct ssl_client;

struct ssl_server *create_ssl_server(void)
	__attribute__ ((malloc));

struct ssl_client *create_ssl_client(void)
	__attribute__ ((malloc));

#ifdef __cplusplus
}
#endif

#endif
