#include <ctype.h>
#include <unistd.h>
#include <libdank/utils/memlimit.h>
#include <libdank/utils/lineparser.h>

int prepare_line_parser(line_parser_ctx *ctx,int fd){
	memset(ctx,0,sizeof(*ctx));
	if((ctx->buf = Malloc("line parser buffer",BUFSIZ)) == NULL){
		return -1;
	}
	ctx->total = BUFSIZ;
	ctx->fd = fd;
	return 0;
}

static inline int
is_comment(const char *buf,size_t offset){
	return *(buf + offset) == '#';
}

// Heap allocates and returns the next line, as delimited by '\n'. Caller must
// free() this result. Comments (lines beginning with optional whitespace and
// a '#' character) are stripped. Returns NULL on end of file or I/O error;
// check errno to differentiate. Errno of 0 indicates standard EOF (success).
char *line_parser_next(line_parser_ctx *ctx){
	unsigned examined = 0;

	while(ctx->count > examined || !ctx->eof){
		char *end;

		// if we've examined all that we've read, we need to read more!
		// if we can't read anymore, return what we have!
		if(ctx->count == examined){
			size_t tlen,uret;
			ssize_t ret;

			if(ctx->count == ctx->total - 1){
				char *tmp;

				if((tmp = Realloc("line parser buf",ctx->buf,ctx->total + BUFSIZ)) == NULL){
					return NULL;
				}
				ctx->total += BUFSIZ;
				ctx->buf = tmp;
			}else if(ctx->count > ctx->base){
				memmove(ctx->buf,ctx->buf + ctx->base,ctx->count);
			}else if(ctx->count){
				memcpy(ctx->buf,ctx->buf + ctx->base,ctx->count);
			}
			ctx->base = 0;
			tlen = ctx->total - ctx->count - 1;
			ret = read(ctx->fd,ctx->buf + ctx->count,tlen);
			if(ret < 0){
				ctx->eof = 1;
				moan("Couldn't read %zu from %d\n",tlen,ctx->fd);
				return NULL;
			}else if((uret = ret) < tlen){
				ctx->eof = 1;
				if(uret == 0){
					break;
				}
			}
			ctx->count += uret;
			ctx->buf[ctx->count] = '\0';
		}
		// we must have something (empty read broke out)
		if(examined == 0){
			int joinlines = 0,joinc = 0,joinb = 0;

			while(ctx->count){
				if(isspace(ctx->buf[ctx->base])){
					if(ctx->buf[ctx->base] == '\n'){
						if(joinlines){
							joinlines = 0;
							ctx->buf[joinb] = ' ';
							ctx->buf[ctx->base] = ' ';
						}
					}
					--ctx->count;
					++ctx->base;
				}else if(ctx->buf[ctx->base] == '\\'){
					if(joinlines){
						ctx->count = joinc;
						ctx->base = joinb;
						break;
					}else{
						joinlines = 1;
						joinc = ctx->count;
						joinb = ctx->base;
					}
				}else{
					if(joinlines){
						ctx->count = joinc;
						ctx->base = joinb;
					}
					break;
				}
			}
			if(ctx->count == 0){
				continue;
			}
		}
		
		// still must have something (whitespace broke out)
		while( (end = strchr(ctx->buf + ctx->base + examined,'\n')) ){
			// safe; if '\n' was first, previous loop got it
			char *tmp = end;
			
			do{
				--tmp;
				if(*tmp == '\\'){
					*tmp = ' ';
					*end = ' ';
					examined += end - (ctx->buf + ctx->base + examined);
					end = NULL;
					break;
				}else if(!isspace(*tmp)){
					break;
				}
			}while(tmp != ctx->buf + ctx->base);
			if(end){
				break;
			}
		}

		if(end){
			if(is_comment(ctx->buf,ctx->base)){
				ctx->count -= (end - (ctx->buf + ctx->base) + 1);
				ctx->base += (end - (ctx->buf + ctx->base) + 1);
				examined = 0;
			}else{
				char *tmp = ctx->buf + ctx->base;

				ctx->count -= (end - (ctx->buf + ctx->base) + 1);
				ctx->base += (end - (ctx->buf + ctx->base) + 1);
				*end = '\0';
				return tmp;
			}
		}else{
			examined = ctx->count;
		}
	}
	
	// we have no data left to read, and it has no \n's; return what we have
	if(ctx->count && !is_comment(ctx->buf,ctx->base)){
		ctx->count = 0;
		return ctx->buf + ctx->base;
	}
	errno = 0;
	return NULL;
}

void destroy_line_parser(line_parser_ctx *ctx){
	Free(ctx->buf);
	memset(ctx,0,sizeof(*ctx));
	ctx->fd = -1;
}

int parser_byline(int fd,line_parser_cb lcb,void *arg){
	int ret = -1,lines = 0;
	line_parser_ctx ctx;
	char *line;

	if(prepare_line_parser(&ctx,fd)){
		return -1;
	}
	if(!lcb){
		nag("Not executing any parser callbacks\n");
	}
	errno = 0;
	while( (line = line_parser_next(&ctx)) ){
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
	destroy_line_parser(&ctx);
	return ret;
}
