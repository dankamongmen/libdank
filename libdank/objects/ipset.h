#ifndef OBJECTS_IPSET
#define OBJECTS_IPSET

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libdank/objects/objustring.h>

// everything in storage is host-byte order, for simpler arithmetic
// (also a speedup over many iterations)
typedef struct iprange {
	uint32_t lower,upper;
} iprange;

int iprange_from_route(iprange *,uint32_t,unsigned);

typedef struct ipset {
	iprange *ranges;
	unsigned rangecount,maxranges;
} ipset;

void init_ipset(ipset *);
void free_ipset(ipset *);

void swap_ipsets(ipset *,ipset *);
int ipsets_equal(const ipset *,const ipset *);
int ipsets_clash(const ipset *,const ipset *);
int clone_ipset(const ipset *,ipset *);
int merge_ipsets(ipset *,const ipset *);
int purge_ipsets(ipset *,const ipset *);
int ipset_is_singleton(const ipset *,uint32_t *);
int iprange_clashes(const ipset *,const iprange *);
int ipset_cmp(const ipset *,const ipset *);
int ipset_encloses(const ipset *,const ipset *);

int parse_ipset(const char *,ipset *);
int stringize_ipset(ustring *,const ipset *);
int parse_initialized_ipset(const char *,ipset *);

uint64_t ipset_area(const ipset *);

static inline int
ip_in_range(const iprange *ir,uint32_t ip){
	return ip >= ir->lower && ip <= ir->upper;
}

static inline int
ipranges_equal(const iprange *i0,const iprange *i1){
	return i0->lower == i1->lower && i0->upper == i1->upper;
}

static inline int
ip_in_set(const ipset *is,uint32_t ip){
	unsigned z;

	for(z = 0 ; z < is->rangecount ; ++z){
		if(ip < is->ranges[z].lower){
			break;
		}
		if(ip <= is->ranges[z].upper){
			return 1;
		}
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
