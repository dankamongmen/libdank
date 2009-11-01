#ifndef ARGUS_NETLINK
#define ARGUS_NETLINK

#include <stdint.h>
#include <libdank/objects/objustring.h>

struct ipset;
struct sockaddr_in;
struct sockaddr_ll;

int init_netlink_layer(void);
int stringize_netinfo(ustring *u);
int stop_netlink_layer(void);

// If there is a single route associated with the ipset, return 0 after setting
// the uint32_t to the source address used with that route. Otherwise, return
// non-zero; there is either no route to some part of the ipset, the ipset is
// broken across multiple routes, or there's multiple routes covering the
// ipset. This information can be invalidated by a routing change following the
// call, or by the route cache.
int srcroute_to_ipset(const struct ipset *,uint32_t *);

int ip_is_local(uint32_t);

unsigned get_maximum_mtu(void);

int setup_sockaddr_ll(const struct sockaddr_in *,struct sockaddr_ll *);

#endif
