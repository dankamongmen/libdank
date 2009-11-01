#ifndef UTILS_WLINEPARSER
#define UTILS_WLINEPARSER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <wchar.h>
#include <libdank/objects/logctx.h>

typedef struct wline_parser_ctx {
	int eof;
	FILE *fp;
	wchar_t *buf;
	unsigned bufroom; // size of buf / sizeof(*buf)
	unsigned bufused; // *buf elements used
} wline_parser_ctx;

typedef int (*wline_parser_cb)(wchar_t *,void *);

// For every wline in the file referred to by fd, call the parser (with arg).
// Return non-zero on any failure, parsing- or system-related.
int parser_bywline(FILE *,wline_parser_cb,void *);

// For objects building atop lineparser
int prepare_wline_parser(wline_parser_ctx *,FILE *);
wchar_t *wline_parser_next(wline_parser_ctx *);
void destroy_wline_parser(wline_parser_ctx *);

#ifdef __cplusplus
}
#endif

#endif
