#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <libdank/utils/memlimit.h>
#include <libdank/utils/wlineparser.h>

int prepare_wline_parser(wline_parser_ctx *ctx,FILE *fp){
	memset(ctx,0,sizeof(*ctx));
	ctx->fp = fp;
	return 0;
}

static int
grow_wline_parser(wline_parser_ctx *ctx,wchar_t wc){
	if(ctx->bufused == ctx->bufroom){
		typeof(*ctx->buf) *tmp;

		if((tmp = Realloc("wbuffer",ctx->buf,(ctx->bufroom + BUFSIZ) * sizeof(*ctx->buf))) == NULL){
			return -1; // -> buf is free()d outside
		}
		ctx->buf = tmp;
		ctx->bufroom += BUFSIZ;
	}
	ctx->buf[ctx->bufused++] = wc;
	return 0;
}

// Heap allocates and returns the next line, as delimited by '\n'. Caller must
// free() this result. Comments (lines beginning with optional whitespace and
// a '#' character) are stripped. Returns NULL on end of file or I/O error;
// check errno to differentiate. Errno of 0 indicates standard EOF (success).
wchar_t *wline_parser_next(wline_parser_ctx *ctx){
	wint_t c;

	if(ctx->eof){
		return NULL;
	}
	ctx->bufused = 0;
	while((c = fgetwc(ctx->fp)) != WEOF){
		wchar_t wc = c;

		if(c == '\n'){
			return grow_wline_parser(ctx,'\0') ? NULL : ctx->buf;
		}
		if(grow_wline_parser(ctx,wc)){
			return NULL;
		}
	}
	ctx->eof = 1;
	if(ctx->bufused == 0){
		return NULL;
	}
	return grow_wline_parser(ctx,'\0') ? NULL : ctx->buf;
}

void destroy_wline_parser(wline_parser_ctx *ctx){
	Free(ctx->buf);
	memset(ctx,0,sizeof(*ctx));
}

int parser_bywline(FILE *fp,wline_parser_cb lcb,void *arg){
	int ret = -1,lines = 0;
	wline_parser_ctx ctx;
	wchar_t *line;

	if(prepare_wline_parser(&ctx,fp)){
		return -1;
	}
	if(!lcb){
		nag("Not executing any parser callbacks\n");
	}
	errno = 0;
	while( (line = wline_parser_next(&ctx)) ){
		++lines;
		if(lcb && lcb(line,arg)){
			bitch("Failure in parse function, exiting\n");
			goto done;
		}
		errno = 0;
	}
	if(errno){
		bitch("Failure reading data for parsing at line %d\n",lines);
	}else{
		nag("Read %d lines\n",lines);
		ret = 0;
	}

done:
	destroy_wline_parser(&ctx);
	return ret;
}
