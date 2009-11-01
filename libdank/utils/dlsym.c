#include <dlfcn.h>
#include <libdank/utils/dlsym.h>
#include <libdank/objects/logctx.h>

#define dlmoan(fmt,...) \
	flog("***** Error (%s) in %s(): "fmt,dlerror(),__func__ ,##__VA_ARGS__)

void *Dlopen(const char *obj,int flags){
	void *ret;

	if((ret = dlopen(obj,flags)) == NULL){
		dlmoan("Couldn't open %s with flags %d\n",obj,flags);
	}
	return ret;
}

// The symbol being looked up might be NULL, thus we need an external method of
// checking for error. We set errstr equal to the string returned by dlerror in
// such a case. Obviously, the resulting content is not and cannot be made
// thread-safe, but the NULL-or-not-NULL-ness can be utilized.
void *Dlsym(void *obj,const char *ident,const char **errstr){
	void *ret;

	// Clear out dlerror()
	dlerror();
	*errstr = NULL;
	ret = dlsym(obj,ident);
	if( (*errstr = dlerror()) ){
		bitch("Couldn't look up %s (%s?)\n",ident,*errstr);
		return NULL;
	}
	return ret;
}

int Dlclose(void *obj){
	int ret;

	if( (ret = dlclose(obj)) ){
		dlmoan("Couldn't close obj %p\n",obj);
	}
	return ret;
}

// On FreeBSD, globally-visible functions from dynamic libraries won't return
// useful data; they'll appear to be _init from the main object. Static objects
// cannot be looked up at all, unless the linker has been instructed to retain
// their symbols.
int Dladdr(void *obj,Dl_info *info){
	int ret;

	// dladdr(3dl) returns non-zero on success, as opposed to failure!
	if((ret = dladdr(obj,info)) == 0){
		dlmoan("Couldn't lookup obj %p\n",obj);
	}
	return ret;
}
