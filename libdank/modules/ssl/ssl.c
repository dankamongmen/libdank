#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/ssl/ssl.h>

static int lock_count;
static pthread_mutex_t *openssl_locks;

struct CRYPTO_dynlock_value {
	pthread_mutex_t mutex; // FIXME use a read-write lock
};

int stop_ssl(void){
	int z,ret = 0;

	RAND_cleanup();
	CRYPTO_set_locking_callback(NULL);
	CRYPTO_set_id_callback(NULL);
	for(z = 0 ; z < lock_count ; ++z){
		if(Pthread_mutex_destroy(&openssl_locks[z])){
			ret = -1;
		}
	}
	lock_count = 0;
	Free(openssl_locks);
	openssl_locks = NULL;
	return ret;
}

// See threads(3SSL)
static void
openssl_lock_callback(int mode,int n,const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	if(n >= lock_count){
		bitch("n too large (%d >= %d)\n",n,lock_count);
		return; // FIXME allow unlocked progression? erp...
	}
	if(mode & CRYPTO_LOCK){
		pthread_mutex_lock(&openssl_locks[n]);
	}else{
		pthread_mutex_unlock(&openssl_locks[n]);
	}
}

static unsigned long
openssl_id_callback(void){
	// FIXME pthread_self() doesn't return an integer type on (at least)
	// FreeBSD...argh :(
	return pthread_self_getnumeric();
}

static struct CRYPTO_dynlock_value *
openssl_dyncreate_callback(const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	struct CRYPTO_dynlock_value *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(pthread_mutex_init(&ret->mutex,NULL)){
			free(ret);
			return NULL;
		}
	}
	return ret;
}

static void
openssl_dynlock_callback(int mode,struct CRYPTO_dynlock_value *l,
				const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	if(mode & CRYPTO_LOCK){
		pthread_mutex_lock(&l->mutex);
	}else{
		pthread_mutex_unlock(&l->mutex);
	}
}

static void
openssl_dyndestroy_callback(struct CRYPTO_dynlock_value *l,
				const char *file __attribute__ ((unused)),
				int line __attribute__ ((unused))){
	pthread_mutex_destroy(&l->mutex);
	free(l);
}

static int
openssl_verify_callback(int preverify_ok,X509_STORE_CTX *xctx __attribute__ ((unused))){
	if(preverify_ok){
		// FIXME check CRL etc
	}else{
		bitch("Preverification failure (preverify_ok == %d)\n",preverify_ok);
	}
	return preverify_ok;
}

SSL_CTX *new_ssl_ctx(const char *certfile,const char *keyfile,const char *cafile){
	SSL_CTX *ret;

	if((ret = SSL_CTX_new(SSLv3_method())) == NULL){
		return NULL;
	}
	// The CA's we trust. We must still ensure the certificate chain is
	// semantically valid, not just syntactically valid! This is done via
	// checking X509 properties and a CRL in openssl_verify_callback().
	if(SSL_CTX_load_verify_locations(ret,cafile,NULL) != 1){
		SSL_CTX_free(ret);
		return NULL;;
	}
	SSL_CTX_set_verify(ret,SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				openssl_verify_callback);
	if(SSL_CTX_use_PrivateKey_file(ret,keyfile,SSL_FILETYPE_PEM) == 1){
		if(SSL_CTX_use_certificate_chain_file(ret,certfile) == 1){
			return ret;
		}
	}else if(SSL_CTX_use_PrivateKey_file(ret,keyfile,SSL_FILETYPE_ASN1) == 1){
		if(SSL_CTX_use_certificate_chain_file(ret,certfile) == 1){
			return ret;
		}
	}
	SSL_CTX_free(ret);
	return NULL;
}

int init_ssl(void){
	int numlocks,z;

	if(SSL_library_init() != 1){
		bitch("Couldn't initialize OpenSSL\n");
		return -1;
	}
	SSL_load_error_strings();
	// OpenSSL transparently seeds the entropy pool on systems supporting a
	// /dev/urandom device. Otherwise, one needs seed it using truly random
	// data from some source (EGD, preserved file, etc). We currently just
	// hope for a /dev/urandom, but at least verify the pool's OK FIXME.
	if(RAND_status() != 1){
		bitch("Entropy pool wasn't seeded (no /dev/urandom?)\n");
		return -1;
	}
	if((numlocks = CRYPTO_num_locks()) < 0){
		RAND_cleanup();
		return -1;
	}else if(numlocks == 0){
		return 0;
	}
	if((openssl_locks = Malloc("OpenSSL locks",sizeof(*openssl_locks) * numlocks)) == NULL){
		RAND_cleanup();
		return -1;
	}
	for(z = 0 ; z < numlocks ; ++z){
		if(Pthread_mutex_init(&openssl_locks[z],NULL)){
			while(z--){
				Pthread_mutex_destroy(&openssl_locks[z]);
			}
			Free(openssl_locks);
			openssl_locks = NULL;
			RAND_cleanup();
			return -1;
		}
	}
	lock_count = numlocks;
	CRYPTO_set_locking_callback(openssl_lock_callback);
	CRYPTO_set_id_callback(openssl_id_callback);
	CRYPTO_set_dynlock_create_callback(openssl_dyncreate_callback);
	CRYPTO_set_dynlock_lock_callback(openssl_dynlock_callback);
	CRYPTO_set_dynlock_destroy_callback(openssl_dyndestroy_callback);
	return 0;
}

SSL *new_ssl(SSL_CTX *ctx){
	return SSL_new(ctx);
}
