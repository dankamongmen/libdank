#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <libdank/utils/string.h>
#include <libdank/ersatz/compat.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>

char *strnchr(char *s,int c,size_t n){
	while(n){
		if(*s == c){
			return s;
		}
		--n;
		++s;
	}
	return NULL;
}

char *strnrchr(char *s,int c,size_t n){
	char *last = NULL;

	while(n){
		if(*s == c){
			last = s;
		}
		--n;
		++s;
	}
	return last;
}

/* strncpy with guaranteed null-termination */
char *sstrncpy(char *dest,const char *src,size_t n) {
	char *ret;

	ret = strncpy(dest,src,n);
	dest[n - 1] = '\0';
	return ret;
}

// n is sizeof dest buf
char *sstrncat(char *dest,const char *src,size_t n) {
	return strncat(dest,src,n - strlen(dest) - 1);
}

// Use brute-force or Crochmere's algorithm (BM, BMH, KMP all use
// preprocessing, which both requires space and time in proportion
// to |needle|. FIXME
#if !defined(LIB_COMPAT_FREEBSD)
const char *strnstr(const char *haystack,const char *needle,size_t len){
	const char *h = haystack,*n = needle;

	// Searching for an empty string in 0 bytes works
	while(*n){
		if(!len-- || !*h){
			return NULL;
		}
		if(*h != *n){
			h = ++haystack;
			len += n - needle;
			n = needle;
			continue;
		}
		++n;
		++h;
	}
	return haystack;
}
#endif

#if !defined(LIB_COMPAT_FREEBSD) && !defined(_GNU_SOURCE)
const char *strcasestr(const char *haystack,const char *needle){
	const char *h = haystack,*n = needle;

	// Searching for an empty string in 0 bytes works
	while(*n){
		if(!*h){
			return NULL;
		}
		if(tolower(*(const unsigned char *)h) != tolower(*(const unsigned char *)n)){
			h = ++haystack;
			n = needle;
			continue;
		}
		++n;
		++h;
	}
	return haystack;
}
#endif

const char *strncasestr(const char *haystack,const char *needle,size_t len){
	const char *h = haystack,*n = needle;

	// Searching for an empty string in 0 bytes works
	while(*n){
		if(!len-- || !*h){
			return NULL;
		}
		if(tolower(*(const unsigned char *)h) != tolower(*(const unsigned char *)n)){
			h = ++haystack;
			len += (n - needle);
			n = needle;
			continue;
		}
		++n;
		++h;
	}
	return haystack;
}

char *Strdup(const char *s){
	char *ret;

	if(s == NULL){
		bitch("Can't duplicate NULL\n");
		return NULL;
	}
	if( (ret = Malloc("string copy",strlen(s) + 1)) ){
		strcpy(ret,s);
	}
	return ret;
}

// only copies, at most, s characters (not including null terminator).
char *Strndup(const char *str,size_t s){
	size_t slen;
	char *ret;

	if(str == NULL){
		bitch("Can't duplicate NULL\n");
		return NULL;
	}
	slen = 0;
	while(slen < s && str[slen]){
		++slen;
	}
	if( (ret = Malloc("bounded string copy",slen + 1)) ){
		if(slen){
			memcpy(ret,str,slen);
		}
		ret[slen] = '\0';
	}
	return ret;
}

// only copies, at most, s characters (not including null terminator).
wchar_t *Wstrndup(const wchar_t *str,size_t s){
	size_t slen;
	wchar_t *ret;

	if(str == NULL){
		bitch("Can't duplicate NULL\n");
		return NULL;
	}
	slen = 0;
	while(slen < s && str[slen]){
		++slen;
	}
	if( (ret = Malloc("bounded wstring copy",(slen + 1) * sizeof(wchar_t))) ){
		if(slen){
			memcpy(ret,str,slen * sizeof(wchar_t));
		}
		ret[slen] = '\0';
	}
	return ret;
}

void *Memdup(const char *obj,const void *src,size_t s){
	void *ret;

	if( (ret = Malloc(obj,s)) ){
		memcpy(ret,src,s);
	}
	return ret;
}
