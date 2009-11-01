#ifndef LIBDANK_UTILS_PROCFS
#define LIBDANK_UTILS_PROCFS

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <libdank/utils/lineparser.h>

char *procfs_cmdline_argv0(pid_t);

int parse_procfile_byline(const char *,line_parser_cb,void *);
int parse_pidprocfile_byline(const char *,pid_t,line_parser_cb,void *);

int procfile_tagged_uint(const char *,const char *,uintmax_t *);
int pidprocfile_tagged_uint(const char *,pid_t,const char *,uintmax_t *);

int procfile_match_line(const char *,const char *,unsigned *);

#ifdef __cplusplus
}
#endif

#endif
