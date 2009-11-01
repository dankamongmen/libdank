#ifndef LIBDANK_MODULES_ADNS_ADNS
#define LIBDANK_MODULES_ADNS_ADNS

#ifdef __cplusplus
extern "C" {
#endif

struct in_addr;

int init_asynchronous_dns(void);	
int stop_asynchronous_dns(void);

int synchronous_dnslookup(const char *,struct in_addr *);

#ifdef __cplusplus
}
#endif

#endif
