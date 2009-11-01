#ifndef UTILS_STRING
#define UTILS_STRING

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <wchar.h>
#include <sys/param.h>
#include <libdank/ersatz/compat.h>

// Orthogonal closures over libc's string functions
char *strnchr(char *,int,size_t);
char *strnrchr(char *,int,size_t);
char *sstrncpy(char *,const char *,size_t);
char *sstrncat(char *,const char *,size_t);

// Two from FreeBSD's libc...
#if !defined(LIB_COMPAT_FREEBSD) && !defined(_GNU_SOURCE)
const char *strcasestr(const char *,const char *);
#endif

#if !defined(LIB_COMPAT_FREEBSD)
const char *strnstr(const char *,const char *,size_t);
#endif

// ...and then this one for orthogonality
const char *strncasestr(const char *,const char *,size_t);

char *Strdup(const char *) __attribute__ ((malloc));
char *Strndup(const char *,size_t) __attribute__ ((malloc));
wchar_t *Wstrndup(const wchar_t *,size_t) __attribute__ ((malloc));
void *Memdup(const char *,const void *,size_t) __attribute__ ((malloc));

#ifdef __cplusplus
}
#endif

#endif
