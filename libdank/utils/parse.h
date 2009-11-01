#ifndef UTILS_PARSE
#define UTILS_PARSE

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>
#include <libdank/objects/portset.h>

static inline int
parse_whitespace(const char **text){
	const char *start = *text;

	while(isspace(**text)){
		++*text;
	}
	return *text - start;
}

static inline int
parse_whitespaces(char **text){
	const char *start = *text;

	while(isspace(**text)){
		++*text;
	}
	return *text - start;
}

static inline int
wparse_whitespace(const wchar_t **text){
	const wchar_t *start = *text;

	while(iswspace(**text)){
		++*text;
	}
	return *text - start;
}

static inline int
wparse_whitespaces(wchar_t **text){
	const wchar_t *start = *text;

	while(iswspace(**text)){
		++*text;
	}
	return *text - start;
}

static inline void
skip_whitespace(const unsigned char **text){
	while(isspace(**text)){
		++*text;
	}
}

static inline void
skip_whitespaces(unsigned char **text){
	while(isspace(**text)){
		++*text;
	}
}

char *parse_next_graph(char *,unsigned *);

int parse_ipv4address(const char *,uint32_t *);

int parse_port(const char *,uint16_t *,int);
int parse_portrange(const char *,portrange *,int);

#ifdef __cplusplus
}
#endif

#endif
