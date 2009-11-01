#ifndef UTILS_LINEPARSER
#define UTILS_LINEPARSER

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/objects/logctx.h>

typedef struct line_parser_ctx {
	int fd;
	char *buf;
	unsigned count,total,base,eof;
} line_parser_ctx;

typedef int (*line_parser_cb)(char *,void *);

// For every line in the file referred to by fd, call the parser (with arg).
// Return non-zero on any failure, parsing- or system-related.
int parser_byline(int,line_parser_cb,void *);

// For objects building atop lineparser
int prepare_line_parser(line_parser_ctx *,int);
char *line_parser_next(line_parser_ctx *);
void destroy_line_parser(line_parser_ctx *);

#ifdef __cplusplus
}
#endif

#endif
