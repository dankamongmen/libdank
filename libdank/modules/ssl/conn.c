#include <libdank/utils/memlimit.h>
#include <libdank/modules/ssl/conn.h>

typedef struct ssl_server {
	int listensd;
} ssl_server;

typedef struct ssl_client {
	int sd;
} ssl_client;

ssl_server *create_ssl_server(void){
	ssl_server *ret;

	if( (ret = Malloc("sslserver",sizeof(*ret))) ){
		// FIXME
		ret->listensd = -1;
	}
	return ret;
}

ssl_client *create_ssl_client(void){
	ssl_client *ret;

	if( (ret = Malloc("sslclient",sizeof(*ret))) ){
		// FIXME
		ret->sd = -1;
	}
	return ret;
}
