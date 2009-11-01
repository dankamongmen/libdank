#ifndef LIBDANK_UTILS_CONFSTR
#define LIBDANK_UTILS_CONFSTR

#ifdef __cplusplus
extern "C" {
#endif

char *confstr_dyn_named(const char *,int);

// for great diagnostic justice
#define confstr_dyn(name) confstr_dyn_named(#name,name)

#ifdef __cplusplus
}
#endif

#endif
