#ifndef UTILS_TOKENPARSER
#define UTILS_TOKENPARSER

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/utils/lineparser.h>

typedef struct token_parser_ctx {
	line_parser_ctx lpc;
	char *lastret,*lastline;
} token_parser_ctx;

int prepare_token_parser_fp(FILE *,token_parser_ctx *);
char *token_parser_next(token_parser_ctx *);
char *token_next_req(token_parser_ctx *,const char *);
void destroy_token_parser(token_parser_ctx *);

#ifdef __cplusplus
}
#endif

#endif
