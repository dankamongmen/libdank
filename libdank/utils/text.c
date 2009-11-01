#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libdank/utils/text.h>
#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

// character set for standard identifiers. we want to give as much flexibility
// as possible, so we use !isspace && isgraph as our baseline acceptor.
static inline int
carve_charset(int x){
	if(isspace(x) || strchr("=\"',",x) || !isgraph(x)){
		return 0;
	}
	return 1;
}

// '\0'-terminates the next standard ident, and returns a pointer to the
// beginning of it. if the token is not followed by space or a '\0', NULL is
// returned. provided success is met, text will point at the remainder of the
// string. skips leading whitespace.
char *carve_token(char **text){
	char *ret,*old;
	int c;

	old = *text;
	parse_whitespaces(text);
	ret = *text;
	while(carve_charset( (c = **text) )){
		++*text;
		if(strchr("(){}[]",c)){
			break;
		}
	}
	if(ret == *text){
		*text = old;
		return NULL;
	}
	if(**text){
		if(!isspace(**text)){
			*text = old;
			return NULL;
		}
		**text = '\0';
		++*text;
	}
	return ret;
}

// take a hardline for right now, but really we just want to avoid things that
// the shell could choke on -- ($, (), [], *, ?, {}, `, etc)
static inline int
shellsafe_charset(int x){
	if(isalnum(x) || x == '-' || x == '_'){
		return 1;
	}
	return 0;
}

// works like carve_token(), but restricts input to alnums, underscore, hyphen
char *carve_shellsafe_token(char **text){
	char *ret,*old;

	old = *text;
	parse_whitespaces(text);
	ret = *text;
	while(shellsafe_charset(**text)){
		++*text;
	}
	if(ret == *text){
		*text = old;
		return NULL;
	}
	if(**text){
		if(!isspace(**text)){
			*text = old;
			return NULL;
		}
		**text = '\0';
		++*text;
	}
	return ret;
}

// carves out a quoted token. text is assumed to be exactly one past the
// initial quote. on non-NULL return, it will be exactly one past where the
// closing quote was (NULL indicates termination without a closing quote). the
// token itself will be newly null-terminated, but this '\0' may not be at
// *text; escaping can shrink the string. currently, blank strings are accepted
// when quoted. the beginning of the carved token is the return value for the
// non-NULL case.
static char *
carve_quoted_token(char **text){
	char *start,*escaped_dest;

	escaped_dest = start = *text;
	while(**text){
		if(**text == '"'){
			if(start == *text){
				bitch("Expected token, got \"\"\n");
				return NULL;
			}
			*escaped_dest = '\0';
			++*text;
			return start;
		}else if(**text == '\\'){
			++*text;
			if(**text == '\0'){
				break;
			}
		}
		// don't start writes until we've had an escaped char
		if(escaped_dest != *text){
			*escaped_dest = **text;
		}
		++escaped_dest;
		++*text;
	}
	return NULL;
}

// will return either a token ala carve_token()'s semantics or, if the first
// char following whitespace is a double quote, use our quoting rules:  an
// empty string cannot be quoted, a backslash escapes any character following
// it, and the full printable charset may be embedded.
char *carve_quotable_token(char **text){
	char *ret,*old;

	old = *text;
	parse_whitespaces(text);
	if(**text == '"'){
		++*text;
		return carve_quoted_token(text);
	}
	ret = *text;
	while(carve_charset(**text) || **text == ','){
		++*text;
	}
	if(ret == *text){
		*text = old;
		return NULL;
	}
	if(**text){
		if(!isspace(**text)){
			*text = old;
			return NULL;
		}
		**text = '\0';
		++*text;
	}
	return ret;
}

// Skips over whitespace, and checks for a field/value pair. This is defined
// as any string a0a1a2..aN=b0b1b2..bN where both carve_charset(aN) and
// !isspace(bN) are true for 0..N.
//
// [exception: b may be surrounded by double quotes, in which case bN!='"'
//  (nested quotes are not supported).]
//
// If such a string is found, a is returned, *value is set to b (excluding
// quotes, if any), and both of these strings are '\0'-delimited. *text is set
// to the character following b's '\0'-delimiter, unless b originally ended the
// string. In the case of a parse error, NULL is returned, with *value left
// undefined, unless we recieved only optional whitespace (in which case &(the
// '\0') is returned and *value is set to NULL).
char *carve_value_pair(char **text,char **value){
	char *toke,*old = *text;

	parse_whitespaces(text);

	// get everything up until '='; return NULL if no text before '='.
	if(*(toke = *text) == '\0'){
		*value = NULL;
		*text = old;
		return toke;
	}
	while(carve_charset(**text)){
		++*text;
	}
	if(*text == toke || **text != '='){
		*text = old;
		return NULL;
	}
	*(*text)++ = '\0';

	// this allows whitespace between = and start of value
	if((*value = carve_quotable_token(text)) == NULL){
		*text = old;
		return NULL;
	}
	return toke;
}

// tries to get a value pair, but settles for a token otherwise.
char *carve_optional_value_pair(char **text,char **value){
	char *ret = carve_value_pair(text,value);

	if(ret == NULL || *value == NULL){
		*value = NULL;
		return carve_token(text);
	}
	return ret;
}

// does *not* skip leading whitespace.  breaks up a series of standard ident
// tokens separated by commas into '\0'-terminated strings.  this series must
// be followed either by a '\0' or whitespace; otherwise, -1 is returned, and
// no changes are made to the text.  no extra-ident characters may appear save
// said ending character.  the number of tokens seen is returned (no tokens at
// all results in a return value of 0).  left is updated to point at the
// remainder of the string, but is undefined if an error is detected.  if left
// is NULL, it goes untouched.
int carve_comma_list(char *text,char **left){
	int tokens = 0,intoken = 0;
	char *cur;

	cur = text;
	while(carve_charset(*cur) || *cur == ','){
		if(*cur == ','){
			++tokens;
			*cur = '\0';
			intoken = 0;
		}else{
			intoken = 1;
		}
		++cur;
	}
	if(*cur){
		if(isspace(*cur)){
			*cur = '\0';
			if(left){
				*left = cur + 1;// point at true remainder
			}
		}else{
			while(tokens){
				if(*--cur == '\0'){
					*cur = ',';
					--tokens;
				}
			}
			return -1;
		}
	}else{
		if(!intoken){	//nothing was here
			return -1;
		}
		++tokens;
		if(left){
			*left = cur;		// point at '\0'-terminator
		}
	}
	return tokens;
}

// sets up an AF_INET sockaddr_in, ready for use with bind(), connect() etc
int carve_ipv4endpoint(struct sockaddr_in *si,char **text,
					unsigned default_port){
	char *start;
	int ret = 0;

	start = *text;
	memset(si,0,sizeof(*si));
	si->sin_family = AF_INET;
	if((ret = parse_ipv4address(*text,&si->sin_addr.s_addr)) < 0){
		return -1;
	}
	*text += ret;
	if(**text == ':'){
		++*text;
	}else{
		si->sin_port = htons(default_port);
		return 0;
	}
	if(carve_single_port(&si->sin_port,text) == NULL){
		*text = start;
		return -1;
	}
	si->sin_port = htons(si->sin_port);
	return 0;
}

uint32_t *carve_single_ip(uint32_t *ip,char **text){
	char ipstr[INET_ADDRSTRLEN],*temp;
	uint32_t s;

	parse_whitespaces(text);
	temp = *text;
	while(isdigit(*temp) || (*temp == '.')){
		++temp;
	}
	if(temp - *text >= INET_ADDRSTRLEN || temp - *text){
		bitch("Invalid IP: %s\n",*text);
		return NULL;
	}
	strncpy(ipstr,*text,(size_t)(temp - *text));
	ipstr[temp - *text] = '\0';
	if(Inet_pton(AF_INET,ipstr,&s)){
		return NULL;
	}
	*ip = s;
	*text = temp;
	return ip;
}

uint16_t *carve_single_port(uint16_t *p,char **text){
	char *digits,*start;
	uint32_t tmp;

	start = *text;
	while(isspace(**text)){
		++*text;
	}
	digits = *text;
	while(isdigit(**text)){
		++*text;
	}
	
	// catch negative sign
	if(digits == *text){
		bitch("Invalid port: %s\n",digits);
		*text = start;
		return NULL;
	}

	// catch huge overflow
	if(*text - digits > 5){
		bitch("Invalid port: %s\n",digits);
		*text = start;
		return NULL;
	}

	// catch num > 16bits
	if(sscanf(digits,"%u",&tmp) != 1 || tmp > 65535){
		bitch("Invalid port: %s\n",digits);
		*text = start;
		return NULL;
	}
	*p = tmp;
	return p;
}
