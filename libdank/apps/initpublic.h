#ifndef APPS_INIT_STDINITPUBLIC
#define APPS_INIT_STDINITPUBLIC

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/version.h>
#include <libdank/apps/init.h>

static inline
int app_init(logctx *lc,const app_def *ad,app_ctx *ac,int argc,
			char **argv,const char *def_lock,const char *def_sock){
	return app_init_vercheck(
#include <libdank/ersatz/svnrev.h>
			,lc,ad,ac,argc,argv,def_lock,def_sock);
}

static inline
int libdank_vercheck(void){
	if(libdank_vercheck_internal(
#include <libdank/ersatz/svnrev.h>
			) == 0){
		timenag("libdank %s rev %s\n",Libdank_Version,LIBDANK_REVISION);
		return 0;
	}
	return -1;
}

#ifdef __cplusplus
}
#endif

#endif
