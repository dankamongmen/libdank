#ifndef UTILS_TEXT
#define UTILS_TEXT

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>

/* destructive text parsing.  for nondestructive, look to parse.h. */

char *carve_token(char **);
char *carve_quotable_token(char **);

char *carve_shellsafe_token(char **);

char *carve_value_pair(char **,char **);
char *carve_optional_value_pair(char **,char **);

int carve_comma_list(char *,char **);

uint32_t *carve_single_ip(uint32_t *,char **);
uint16_t *carve_single_port(uint16_t *,char **);

int carve_ipv4endpoint(struct sockaddr_in *,char **,unsigned);

#ifdef __cplusplus
}
#endif

#endif
