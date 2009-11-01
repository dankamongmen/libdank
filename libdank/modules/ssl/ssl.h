#ifndef LIBDANK_MODULES_SSL_SSL
#define LIBDANK_MODULES_SSL_SSL

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>

int init_ssl(void);
int stop_ssl(void);
SSL_CTX *new_ssl_ctx(const char *,const char *,const char *);
SSL *new_ssl(SSL_CTX *);

#ifdef __cplusplus
}
#endif

#endif
