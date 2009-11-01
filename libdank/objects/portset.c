#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/portset.h>

static const portrange PORTRANGE_ALL = {
	.lower = 0,
	.upper = 0xffff,
};

void init_portrange(portrange *i,unsigned lower,unsigned upper){
	if(lower > upper){
		i->lower = upper;
		i->upper = lower;
	}else{
		i->lower = lower;
		i->upper = upper;
	}
}

void init_portset(portset *p){
	memset(p,0,sizeof(*p));
}

void free_portset(portset *p){
	if(p){
		Free(p->ranges);
		init_portset(p);
	}
}

int portset_empty(const portset *ps){
	return ps->rangecount == 0;
}

int portset_complete(const portset *ps){
	if(ps->rangecount == 1){
		portrange *r = &ps->ranges[0];

		return r->lower == 0 && r->upper == 65535;
	}
	return 0;
}

uint16_t portset_portcount(const portset *ps){
	uint16_t count = 0;
	unsigned rangeidx;

	for(rangeidx = 0 ; rangeidx < ps->rangecount ; ++rangeidx){
		count += ps->ranges[rangeidx].upper - ps->ranges[rangeidx].lower + 1;
	}
	return count;
}

void init_portset_iterator(const portset *ps,portset_iterator *psi){
	psi->ps = ps;
	psi->pridx = 0;
	psi->priter = -1;
}

int portset_iterate(portset_iterator *psi,unsigned *port){
	const portrange *pr;

	if(psi->priter < 0){ // new pridx, new pr, check for validity;
		if(psi->pridx >= psi->ps->rangecount){
			return 0;
		}
		psi->priter = 0;
	}
	pr = psi->ps->ranges + psi->pridx;
	*port = pr->lower + psi->priter;
	if(++psi->priter > pr->upper - pr->lower){
		psi->priter = -1;
	}
	return 1;
}

static int
stringize_portrange(ustring *u,const portrange *pr){
	if(printUString(u,"%hu",pr->lower) < 0){
		return -1;
	}
	if(pr->lower != pr->upper){
		if(printUString(u,"-%hu",pr->upper) < 0){
			return -1;
		}
	}
	return 0;
}

int stringize_portset(ustring *u,const portset *ps){
	const char *name = NULL;
	unsigned z = 0;

	if(portset_empty(ps)){
		name = "none";
	}else if(portset_complete(ps)){
		name = "any";
	}
	if(name){
		if(printUString(u,"%s",name) < 0){
			return -1;
		}
		return 0;
	}
	for(z = 0 ; z < ps->rangecount ; ++z){
		if(z && printUString(u,",") < 0){
			return -1;
		}
		if(stringize_portrange(u,&ps->ranges[z])){
			return -1;
		}
	}
	return 0;
}

static int
extend_portset(portset *p){
	portrange *tmp;
	size_t s;

	s = sizeof(*tmp) * (p->rangecount + 1);
	if((tmp = Realloc("portset array",p->ranges,s)) == NULL){
		return -1;
	}
	p->ranges = tmp;
	++p->rangecount;
	return 0;
}

static int
append_portset(portset *p,const portrange *pr){
	if(extend_portset(p)){
		return -1;
	}
	p->ranges[p->rangecount - 1] = *pr;
	return 0;
}

static int
insert_portset(portset *p,unsigned pre,const portrange *pr){
	portrange *targ,*cur;
	size_t s;

	if(extend_portset(p)){
		return -1;
	}
	cur = &p->ranges[pre];
	targ = &p->ranges[pre + 1];
	s = sizeof(*targ) * (p->rangecount - pre - 1);
	memmove(targ,cur,s);
	p->ranges[pre] = *pr;
	return 0;
}

static void
remove_portset(portset *p,unsigned at,unsigned count){
	if(p->rangecount > at + count){
		memmove(&p->ranges[at],&p->ranges[at + count],
				sizeof(*p->ranges) * (p->rangecount - (at + count)));
	}
	if((p->rangecount -= count) == 0){
		Free(p->ranges);
		p->ranges = NULL;
	}
}

static int
merge_portset(portset *p,const portrange *merge){
	portrange *cur;
	unsigned z = 0;

	// loop over all portranges we cannot merge with due to their highest
	// port being less than our lowest - 1 (if equal, they absorb us).
	while(z != p->rangecount){
		cur = &p->ranges[z];
		if(merge->lower <= cur->upper + 1 || cur->upper == 0xffff){
			break;
		}
		++z;
	}

	// if z == p->rangecount, our lower port is the highest in the list.
	if(z == p->rangecount){
		return append_portset(p,merge);
	}
	
	// we know now cur->upper + 1 >= merge->lower.

	// we can't merge with this; our high element is too small.
	if(merge->upper + 1 < cur->lower && merge->upper){
		return insert_portset(p,z,merge);
	}

	// if we enclose them on the lower end, grow them down
	if(merge->lower <= cur->lower - 1 && cur->lower){
		cur->lower = merge->lower;
	}

	// Their lower is correct; we must see if we engulf them on the upper
	// end. If so, we may have to "eat" others, as well.
	if(merge->upper > cur->upper){
		portrange *tmp = cur;
		unsigned zin = ++z;

		cur->upper = merge->upper;
		// loop until we can't merge their lower end
		while(zin < p->rangecount){
			++tmp;
			if(cur->upper < tmp->lower - 1 && tmp->lower){
				break;
			}
			if(tmp->upper > cur->upper){
				cur->upper = tmp->upper;
			}
			++zin;
		}
		if(zin > z){
			remove_portset(p,z,zin - z);
		}
	}
	return 0;
}

int merge_portsets(portset *portsum,const portset *add){
	portset pstmp;
	unsigned z;

	if(clone_portset(&pstmp,portsum)){
		return -1;
	}
	for(z = 0 ; z < add->rangecount ; ++z){
		if(merge_portset(&pstmp,&add->ranges[z])){
			free_portset(&pstmp);
			return -1;
		}
	}
	free_portset(portsum);
	*portsum = pstmp;
	return 0;
}

int cut_portset(portset *p,const portrange *cut){
	unsigned z = 0;

	while(z < p->rangecount){
		portrange *cur = &p->ranges[z];

		if(cut->lower > cur->upper || cut->upper < cur->lower){
			++z;
			continue;
		}
		if(cut->lower <= cur->lower){
			if(cur->upper > cut->upper){
				cur->lower = cut->upper + 1;
				break;
			}else{
				remove_portset(p,z,1);
				continue;
			}
		}
		if(cut->upper >= cur->upper){
			cur->upper = cut->lower - 1;
			++z;
		}else{
			unsigned l = cur->lower,u = cut->lower - 1;
			portrange pr;
			
			init_portrange(&pr,l,u);
			if(insert_portset(p,z,&pr)){
				return -1;
			}
			cur = &p->ranges[z + 1];
			cur->lower = cut->upper + 1;
			break;
		}
	}
	return 0;
}

int portsets_equal(const portset *p0,const portset *p1){
	size_t s;

	if(p0->rangecount != p1->rangecount){
		return 0;
	}
	if(p0->rangecount == 0){
		return 1;
	}
	s = sizeof(*p0->ranges) * p0->rangecount;
	return !memcmp(p0->ranges,p1->ranges,s);
}

void swap_portsets(portset *p0,portset *p1){
	portrange *ranges;
	unsigned rcount;

	ranges = p0->ranges;
	rcount = p0->rangecount;
	p0->ranges = p1->ranges;
	p0->rangecount = p1->rangecount;
	p1->ranges = ranges;
	p1->rangecount = rcount;
}

int contains_portset(const portset *super,const portset *sub){
	unsigned subz;

	for(subz = 0 ; subz < sub->rangecount ; ++subz){
		const portrange *subpr;
		unsigned superz;

		subpr = &sub->ranges[subz];
		for(superz = 0 ; superz < super->rangecount ; ++superz){
			const portrange *superpr;

			superpr = &sub->ranges[superz];
			if(port_in_range(superpr,subpr->lower)){
				if(port_in_range(superpr,subpr->upper)){
					break;
				}
			}
		}
		if(superz == super->rangecount){
			return 0;
		}
	}
	return 1;
}

static portset *
parse_numeric_ports(const char **buf){
	const char *start = *buf;
	int i,need_portset = 1;
	portset tmp,*ps;
	portrange pr;

	init_portset(&tmp);
	while((i = parse_portrange(*buf,&pr,!need_portset)) >= 0){
		if(merge_portset(&tmp,&pr)){
			free_portset(&tmp);
			*buf = start;
			return NULL;
		}
		if(*(*buf += i) == ','){
			++*buf;
		}else{
			need_portset = 0;
		}
	}
	if(need_portset){
		bitch("Parse error: expected portrange, got %s\n",start);
		free_portset(&tmp);
		*buf = start;
		return NULL;
	}
	if((ps = Memdup("port set copy",&tmp,sizeof(tmp))) == NULL){
		free_portset(&tmp);
	}
	return ps;
}

int clone_portset(portset *dst,const portset *src){
	size_t heap;

	if((dst->rangecount = src->rangecount) == 0){
		dst->ranges = NULL;
		return 0;
	}
	heap = sizeof(*src->ranges) * src->rangecount;
	if((dst->ranges = Memdup("portset copy",src->ranges,heap)) == NULL){
		return -1;
	}
	dst->rangecount = src->rangecount;
	return 0;
}

// ports must have been initialized and operated on properly. Normally, the
// input var will simply have had init_portset() called on it.
int parse_portset(const char *text,portset *ports){
	const char *start = text;
	int invert = 0;
	portset *ps;

	parse_whitespace(&text);
	if(*text == '!'){
		invert = 1;
		++text;
	}
	parse_whitespace(&text);
	// special-case keywords. numeric ranges can equal "any" but not "none"
	#define ANY "any"
	if(strncasecmp(text,ANY,__builtin_strlen(ANY)) == 0){
		if(invert){
			bitch("Applied ! operator to %s\n",text);
			goto err;
		}
		if(merge_portset(ports,&PORTRANGE_ALL)){
			goto err;
		}
		return text - start + __builtin_strlen(ANY);
	}
	#undef ANY
	#define NONE "none"
	if(strncasecmp(text,NONE,__builtin_strlen(NONE)) == 0){
		if(invert){
			bitch("Applied ! operator to %s\n",text);
			goto err;
		}
		return text - start + __builtin_strlen(NONE);
	}
	#undef NONE
	if((ps = parse_numeric_ports(&text)) == NULL){
		goto err;
	}
	if(invert){
		unsigned z;

		if(merge_portset(ports,&PORTRANGE_ALL)){
			free_portset(ps);
			Free(ps);
			goto err;
		}
		for(z = 0 ; z < ps->rangecount ; ++z){
			if(cut_portset(ports,&ps->ranges[z])){
				free_portset(ps);
				Free(ps);
				goto err;
			}
		}
		free_portset(ps);
	}else{
		free_portset(ports);
		memcpy(ports,ps,sizeof(*ports));
	}
	Free(ps);

	if(ports->rangecount == 0){
		bitch("Expected portset, got empty range at %s\n",start);
		return -1;
	}
	return text - start;

err:
	free_portset(ports);
	return -1;
}

int portset_cmp(const portset *ps0,const portset *ps1){
	unsigned z;

	for(z = 0 ; z < ps0->rangecount && z < ps1->rangecount ; ++z){
		int ret;

		if( (ret = ps0->ranges[z].lower < ps1->ranges[z].lower ? -1 :
			ps0->ranges[z].lower > ps1->ranges[z].lower ? 1 :
			ps0->ranges[z].upper < ps1->ranges[z].upper ? -1 :
			ps0->ranges[z].upper > ps1->ranges[z].upper) ){
			return ret;
		}
	}
	return ps0->rangecount < ps1->rangecount ? -1 :
		ps0->rangecount > ps1->rangecount;
}
