#include <string.h>
#include <pthread.h>
#include <libdank/utils/syswrap_r.h>

static pthread_mutex_t locale_lock = PTHREAD_MUTEX_INITIALIZER;

// Returns the number of bytes required to copy the string plus its null
// terminator, or 0 if nl_langinfo(3) returns NULL (an empty string will return
// 1, since there will be a byte required for the terminator). If s is less
// than the return value, the result has been truncated. The result will always
// be null terminated, unless NULL and thus 0 is returned, and no changes made.
size_t Nl_langinfo_r(nl_item nlitem,char *buf,size_t s){
	size_t len = 0;
	char *ret;

	pthread_mutex_lock(&locale_lock);
	if( (ret = nl_langinfo(nlitem)) ){
		if((len = strlen(ret)) <= s){
			memcpy(buf,ret,len);
		}else{
			memcpy(buf,ret,s - 1);
			buf[s - 1] = '\0';
		}
	}
	pthread_mutex_unlock(&locale_lock);
	return len;
}
