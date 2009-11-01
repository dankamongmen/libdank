#ifndef LIBDANK_MODULES_SSL_CRL
#define LIBDANK_MODULES_SSL_CRL

#include <openssl/ssl.h>

X509_CRL *open_x509_crl_pemfile(const X509_STORE_CTX *,const char *);
int is_x509_cert_revoked(const X509_CRL *,const X509 *);

#endif
