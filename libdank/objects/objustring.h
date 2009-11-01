#ifndef OBJECTS_OBJUSTRING
#define OBJECTS_OBJUSTRING

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

typedef struct ustring {
	char *string;
	size_t current,total;
} ustring;

// This can't be used in C++ due to lack of designated initializers (as
// introduced by ISO C99). Use init_ustring(), instead.
#define USTRING_INITIALIZER { .string = NULL, .current = 0, .total = 0 }

ustring *create_ustring(void);
void free_ustring(ustring **);

void init_ustring(ustring *);
void reset_ustring(ustring *);

int printUString(ustring *,const char *,...)
	__attribute__ ((format (printf,2,3)));

int vprintUString(ustring *,const char *,va_list);

#ifdef __cplusplus
}
#endif

#endif
