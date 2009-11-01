#include <stdlib.h>
#include <libdank/apps/stop.h>
#include <libdank/utils/pidlock.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/objustring.h>
#include <libdank/modules/fileconf/sbox.h>
#include <libdank/modules/logging/logging.h>
#include <libdank/modules/ctlserver/ctlserver.h>

void app_stop(const app_def *app,app_ctx *ctx,int ret){
	ustring u = USTRING_INITIALIZER;

	if(app){
		ret |= stop_fileconf();
	}
	if(ctx){
		if(ctx->ctlsrvsocket){
			ret |= stop_log_server();
			ret |= stop_ctlserver();
			ctx->ctlsrvsocket = NULL;
		}
		if(ctx->lockfile){
			ret |= purge_lockfile(ctx->lockfile);
			free(ctx->lockfile);
			ctx->lockfile = NULL;
		}
	}
	if(stringize_memory_usage(&u) == 0){
		nag("%s\n",u.string);
	}
	reset_ustring(&u);
	ret = ret ? EXIT_FAILURE : EXIT_SUCCESS;
	stop_logging(ret);
	exit(ret);
}
