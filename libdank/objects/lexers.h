#ifndef LIBDANK_OBJECTS_LEXERS
#define LIBDANK_OBJECTS_LEXERS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Call as lex_uBITS(&token,&val) ie: lex_u32(&token,&val)
#define lex_uint_typewrapper_export(bits) \
int lex_u##bits(const char **,uint##bits##_t *); \
int lex_u##bits##_ashex(const char **,uint##bits##_t *);

lex_uint_typewrapper_export(8)
lex_uint_typewrapper_export(16)
lex_uint_typewrapper_export(32)
lex_uint_typewrapper_export(64)
lex_uint_typewrapper_export(ptr)
lex_uint_typewrapper_export(max)
#undef lex_uint_typewrapper_export

#define lex_sint_typewrapper_export(bits) \
int lex_s##bits(const char **,int##bits##_t *);

// Two's complement gives you [-2^n-1 .. 2^n-1), but we only (-2^n-1 .. 2^n-1).
lex_sint_typewrapper_export(8)
lex_sint_typewrapper_export(16)
lex_sint_typewrapper_export(32)
lex_sint_typewrapper_export(64)
lex_sint_typewrapper_export(ptr)
lex_sint_typewrapper_export(max)
#undef lex_sint_typewrapper_export

#ifdef __cplusplus
}
#endif

#endif
