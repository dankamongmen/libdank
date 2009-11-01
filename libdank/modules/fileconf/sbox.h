#ifndef OBJECTS_SBOX
#define OBJECTS_SBOX

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/utils/lineparser.h>
#include <libdank/utils/wlineparser.h>

struct _xmlDoc;
struct config_data;

int init_fileconf(const char *);
int stop_fileconf(void);

struct config_data *open_config(const char *);

int parse_config(struct config_data *,line_parser_cb,void *);
int parse_wconfig(struct config_data *,wline_parser_cb,void *);
int parse_config_xmlfile(struct config_data *,struct _xmlDoc **);

int reparse_config(line_parser_cb,void *,FILE *);
int reparse_xmlconf(struct _xmlDoc **,FILE *);

void free_config_data(struct config_data **);

#ifdef __cplusplus
}
#endif

#endif
