#include <errno.h>
#include <unistd.h>
#include <libdank/utils/confstr.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

char *confstr_dyn_named(const char *named,int name){
	char *buf = NULL;
	size_t l;

	errno = 0;
	if( (l = confstr(name,buf,0)) ){
		if( (buf = Malloc("confstring",l)) ){
			if(confstr(name,buf,l) != l){
				bitch("Treason uncloaked: confstr ret != %zu for %s (%d)\n",
						l,named,name);
				Free(buf);
				buf = NULL;
			}
		}else{
			bitch("Couldn't allocate for %s (%d)\n",named,name);
		}
	}else if(errno == EINVAL){
		bitch("Invalid configuration variable: %s (%d)\n",named,name);
	}else{
		nag("Configuration variable had no value: %s (%d)\n",named,name);
	}
	return buf;
}
