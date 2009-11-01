#include <libdank/utils/syswrap.h>
#include <libdank/modules/ssl/crl.h>

static inline char *
copy_x509_issuer_name(X509 *x509){
	if(x509){
		X509_NAME *xname;
	       
		if( (xname = X509_get_issuer_name(x509)) ){
			return X509_NAME_oneline(xname,NULL,0);
		}
	}
	return NULL;
}

static inline char *
copy_x509crl_issuer_name(X509_CRL *x509){
	if(x509){
		X509_NAME *xname;
	       
		if( (xname = X509_CRL_get_issuer(x509)) ){
			return X509_NAME_oneline(xname,NULL,0);
		}
	}
	return NULL;
}

X509_CRL *open_x509_crl_pemfile(const X509_STORE_CTX *xctx,const char *crlfile){
	char *ourissue;
	X509_CRL *crl;
	FILE *fp;

	if(xctx == NULL){
		return NULL;
	}
	if((ourissue = copy_x509_issuer_name(xctx->current_cert)) == NULL){
		return NULL;
	}
	if((fp = Fopen(crlfile,"rb")) == NULL){
		free(ourissue);
		return NULL;
	}
	if( (crl = PEM_read_X509_CRL(fp,&crl,NULL,NULL)) ){
		if(X509_NAME_cmp(X509_CRL_get_issuer(crl),X509_get_issuer_name(xctx->current_cert))){
			char *crlissue;

			if( (crlissue = copy_x509crl_issuer_name(crl)) ){
				bitch("CRL issuer (%s) didn't match cert (%s)\n",crlissue,ourissue);
				free(crlissue);
			}else{
				bitch("CRL issuer didn't match cert (%s)\n",ourissue);
			}
			X509_CRL_free(crl);
			crl = NULL;
		}else{
			nag("Verified CRL (Issuer: '%s')\n",ourissue);
		}
	}
	fclose(fp);
	free(ourissue);
	return crl;
}

int is_x509_cert_revoked(const X509_CRL *crl,const X509 *cert){
	if(crl == NULL || cert == NULL){
		return -1;
	}
	// FIXME
	return 0;
}
