#ifndef OBJECTS_PORTSET
#define OBJECTS_PORTSET

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libdank/objects/objustring.h>

// everything stores in and expects host byte order.

// array of ports to eliminate data dependence in traversal.
typedef struct portrange {
	uint16_t lower,upper;
} portrange;

typedef struct portset {
	unsigned rangecount;
	portrange *ranges;
} portset;

void init_portrange(portrange *,unsigned,unsigned);

void init_portset(portset *);
void free_portset(portset *);

int portset_empty(const portset *);
int portset_complete(const portset *);
uint16_t portset_portcount(const portset *);

int parse_portset(const char *,portset *);
int portsets_equal(const portset *,const portset *);
int clone_portset(portset *,const portset *);
int cut_portset(portset *,const portrange *);
int contains_portset(const portset *,const portset *);
int merge_portsets(portset *,const portset *);
int stringize_portset(ustring *,const portset *);
int portset_cmp(const portset *,const portset *);
void swap_portsets(portset *,portset *);

// Use of & below rather than && is not an error, but an optimization
static inline
int port_in_range(const portrange *p,unsigned port){
        return (port >= p->lower) & (port <= p->upper);
}

static inline
int contains_port(const portset *p,uint16_t port){
	unsigned z;

	for(z = 0 ; z < p->rangecount ; ++z){
		if(port < p->ranges[z].lower){
			break;
		}
		if(port <= p->ranges[z].upper){
			return 1;
		}
	}
	return 0;
}

typedef struct portset_iterator {
	const portset *ps;
	int priter;
	unsigned pridx,cur;
} portset_iterator;

void init_portset_iterator(const portset *,portset_iterator *);
int portset_iterate(portset_iterator *,unsigned *);

#ifdef __cplusplus
}
#endif

#endif
