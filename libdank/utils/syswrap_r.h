#ifndef UTILS_SYSWRAP_R
#define UTILS_SYSWRAP_R

#ifdef __cplusplus
extern "C" {
#endif

#include <langinfo.h>

size_t Nl_langinfo_r(nl_item,char *,size_t);

#ifdef __cplusplus
}
#endif

#endif
