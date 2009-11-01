#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/objustring.h>

ustring *create_ustring(void){
	ustring *ret;

	if( (ret = Malloc("ustring",sizeof(*ret))) ){
		init_ustring(ret);
	}
	return ret;
}

void init_ustring(ustring *u){
	memset(u,0,sizeof(*u));
}

void reset_ustring(ustring *u){
	if(u){
		Free(u->string);
		memset(u,0,sizeof(*u));
	}
}

void free_ustring(ustring **u){
	if(u && *u){
		reset_ustring(*u);
		Free(*u);
		*u = NULL;
	}
}

int printUString(ustring *u,const char *fmt,...){
	int r;
	va_list ap;

	va_start(ap, fmt);
		r = vprintUString(u,fmt,ap);
	va_end(ap);
	return r;
}

int vprintUString(ustring *u,const char *fmt,va_list v){
	logctx *lc;
	va_list tv;
	int ret;

	lc = get_thread_logctx();
	va_copy(tv,v);
	// Old (pre-ANSI, basically) snprintf() implementations return -1 instead of
	// the number of characters (not including the terminator) that would be
	// printed. In that case, we normalize to 1 -- this means we must always grow
	// more quickly than the normalized return value (as we would anyway), lest
	// such an implementation require O(N) allocs!
	while((ret = vsnprintf(u->string + u->current,u->total - u->current,fmt,tv)) < 0
			|| (size_t)ret >= u->total - u->current){
		ustring *out = NULL,*err = NULL;
		size_t heap;
		char *tmp;

		va_end(tv);
		if(lc){
			if((out = lc->out) == u){
				lc->out = NULL;
			}
			if((err = lc->err) == u){
				lc->err = NULL;
			}
		}
		if(ret < 0){
			ret = 1;
		}
		heap = u->total + ret + BUFSIZ;
		if((tmp = Realloc("string extension",u->string,heap)) == NULL){
			if(u->total != u->current){
				u->string[u->current] = '\0';
			}
			if(lc){
				lc->err = err;
				lc->out = out;
			}
			return -1;
		}
		u->total = heap;
		u->string = tmp;
		if(lc){
			lc->err = err;
			lc->out = out;
		}
		va_copy(tv,v);
	}
	va_end(tv);
	u->current += ret;
	return ret;
}
