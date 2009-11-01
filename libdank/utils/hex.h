#ifndef UTILS_HEX
#define UTILS_HEX

#include <libdank/objects/objustring.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *hextoascii(const char * restrict,unsigned char * restrict,
				int,size_t);
void asciitohex(const void * restrict,char * restrict,int,size_t);
int us_asciitohex(const void * restrict,ustring * restrict,int,size_t);

#ifdef __cplusplus
}
#endif

#endif
