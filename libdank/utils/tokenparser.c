#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/utils/tokenparser.h>

int prepare_token_parser_fp(FILE *fp,token_parser_ctx *tpc){
	int fd;

	if((fd = Fileno(fp)) < 0){
		return -1;
	}
	tpc->lastline = tpc->lastret = NULL;
	return prepare_line_parser(&tpc->lpc,fd);
}

static inline int
token_continuator(int c){
	return (isalnum(c) || c == '_' || c == '-' || c == '/' ||
			c == ':' || c == '.' || c == '@');
}

char *token_parser_next(token_parser_ctx *tpc){
	char *ret,*end;

	Free(tpc->lastret);
	tpc->lastret = NULL;
	if(tpc->lastline){
		parse_whitespaces(&tpc->lastline);
		if(*tpc->lastline == '\0'){
			tpc->lastline = NULL;
		}
	}
	if(tpc->lastline == NULL){
		errno = 0;
		if((tpc->lastline = line_parser_next(&tpc->lpc)) == NULL){
			return NULL;
		}
		parse_whitespaces(&tpc->lastline);
	}
	end = tpc->lastline + 1;
	if(*tpc->lastline == '"'){
		++tpc->lastline;
		// need some type of escaping FIXME
		while(*end != '"'){
			++end;
		}
		ret = Strndup(tpc->lastline,(size_t)(end - tpc->lastline));
		if(ret == NULL){
			return NULL;
		}
		tpc->lastline = end + 1;
		return tpc->lastret = ret;
	}else if(token_continuator(*tpc->lastline)){
		while(token_continuator(*end)){
			++end;
		}
	}
	ret = Strndup(tpc->lastline,(size_t)(end - tpc->lastline));
	if(ret == NULL){
		return NULL;
	}
	tpc->lastline = end;
	return tpc->lastret = ret;
}

char *token_next_req(token_parser_ctx *tpc,const char *reqd){
	char *t;

	if((t = token_parser_next(tpc)) == NULL){
		if(!errno){
			bitch("Expected %s, got EOF\n",reqd);
		}
	}
	return t;
}

void destroy_token_parser(token_parser_ctx *tpc){
	if(tpc){
		Free(tpc->lastret);
		destroy_line_parser(&tpc->lpc);
	}
}
