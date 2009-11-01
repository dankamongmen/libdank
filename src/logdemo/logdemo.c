#include <stdlib.h>
#include <libdank/apps/stop.h>
#include <libdank/utils/syswrap.h>
#include <libdank/apps/initpublic.h>
#include <libdank/modules/tracing/threads.h>

#define APPNAME "logdemo"

static const app_def appdef = {
	.appname = APPNAME,
};

static void
helper(void *arg){
	nag("Argument address: %p\n",arg);
}

int main(int argc,char **argv){
	app_ctx appctx;
	pthread_t tid;
	logctx lc;

	memset(&appctx,0,sizeof(appctx));
	if(app_init(&lc,&appdef,&appctx,argc,argv,NULL,NULL)){
		return EXIT_FAILURE;
	}
	if(new_traceable_thread("helper",&tid,helper,NULL)){
		app_stop(&appdef,&appctx,EXIT_FAILURE);
	}
	if(join_traceable_thread("helper",tid)){
		app_stop(&appdef,&appctx,EXIT_FAILURE);
	}
	app_stop(&appdef,&appctx,EXIT_SUCCESS);
}
