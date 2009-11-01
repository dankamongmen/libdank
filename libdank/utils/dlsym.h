#ifndef LIBDANK_UTILS_DLSYM
#define LIBDANK_UTILS_DLSYM

#ifdef __cplusplus
extern "C" {
#endif

#include <dlfcn.h>

void *Dlopen(const char *,int);
void *Dlsym(void *,const char *,const char **);
int Dlclose(void *);

#if defined(__GLIBC__) || defined(__FreeBSD__)
int Dladdr(void *,Dl_info *);
#endif

#ifdef __cplusplus
}
#endif

#endif
