#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/objects/ipset.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>

static const iprange IPRANGE_ALL = {
	.lower = 0,
	.upper = 0xffffffff,
};

static void
init_iprange(iprange *i,uint32_t lower,uint32_t upper){
	if(lower > upper){
		i->lower = upper;
		i->upper = lower;
	}else{
		i->lower = lower;
		i->upper = upper;
	}
}

static int
stringize_iprange(ustring *u,const iprange *ir){
	char buf[INET_ADDRSTRLEN];
	uint32_t tmp;

	tmp = htonl(ir->lower);
	if(inet_ntop(AF_INET,&tmp,buf,INET_ADDRSTRLEN) == NULL){
		return -1;
	}
	if(printUString(u,"%s",buf) < 0){
		return -1;
	}
	if(ir->upper != ir->lower){
		tmp = htonl(ir->upper);
		if(inet_ntop(AF_INET,&tmp,buf,INET_ADDRSTRLEN) == NULL){
			return -1;
		}
		if(printUString(u,"-%s",buf) < 0){
			return -1;
		}
	}
	return 0;
}

int stringize_ipset(ustring *u,const ipset *is){
	unsigned z;

	if(printUString(u,"%c",'[') < 0){
		return -1;
	}
	for(z = 0 ; z < is->rangecount ; ++z){
		iprange *cur = &is->ranges[z];

		if(z && printUString(u,",") < 0){
			return -1;
		}
		if(stringize_iprange(u,cur)){
			return -1;
		}
	}
	if(printUString(u,"%c",']') < 0){
		return -1;
	}
	return 0;
}

static int
parse_ipv4range(const char *buf,iprange *ir){
	const char *start = buf;
	int i;

	parse_whitespace(&buf);
	if(*buf == '-' || *buf == ':'){
		++buf;
		ir->lower = 0;
		if((i = parse_ipv4address(buf,&ir->upper)) <= 0){
			goto err;
		}
		buf += i;
		goto done;
	}
	if((i = parse_ipv4address(buf,&ir->lower)) <= 0){
		goto err;
	}
	buf += i;
	ir->lower = ntohl(ir->lower);
	if(*buf == '/'){
		++buf;
		parse_whitespace(&buf);
		if(sscanf(buf,"%d",&i) != 1){
			goto err;
		}
		if(i > 0){
			ir->lower &= (~0U) << (32 - i);
			ir->upper = ir->lower | ((1U << (32 - i)) - 1);
		}else if(i == 0){
			ir->lower = htonl(0U);
			ir->upper = htonl(~0U);
		}else{
			goto err;
		}
		while(isdigit(*buf)){
			++buf;
		}
	}else if(*buf == '-' || *buf == ':'){
		++buf;
		if(!isdigit(*buf)){
			ir->upper = htonl(~0U);
			goto done;
		}
		if((i = parse_ipv4address(buf,&ir->upper)) <= 0){
			goto err;
		}
		buf += i;
		ir->upper = ntohl(ir->upper);
		if(ir->lower > ir->upper){
			uint32_t ut;

			ut = ir->lower;
			ir->lower = ir->upper;
			ir->upper = ut;
		}
	}else{
		ir->upper = ir->lower;
	}

done:
	if(isspace(*buf) || *buf == ',' || *buf == '\0' || *buf == ']' || *buf == '@'){
		return buf - start;
	}

err:
	bitch("Wanted IPv4 address range, got %s\n",start);
	return -1;
}

static int
extend_ipset(ipset *i){
	if(i->rangecount == i->maxranges){
		iprange *tmp;
		size_t s;

		s = sizeof(*i->ranges) * (i->rangecount + 1);
		if((tmp = Realloc("ipset array",i->ranges,s)) == NULL){
			return -1;
		}
		i->ranges = tmp;
		i->maxranges = i->rangecount + 1;
	}
	++i->rangecount;
	return 0;
}

static int
append_ipset(ipset *i,const iprange *ir){
	if(extend_ipset(i)){
		return -1;
	}
	i->ranges[i->rangecount - 1] = *ir;
	return 0;
}

static int
insert_ipset(ipset *i,unsigned pre,const iprange *ir){
	iprange *targ,*cur;
	size_t s;

	if(extend_ipset(i)){
		return -1;
	}
	cur = &i->ranges[pre];
	targ = &i->ranges[pre + 1];
	s = sizeof(*targ) * (i->rangecount - pre - 1);
	memmove(targ,cur,s);
	i->ranges[pre] = *ir;
	return 0;
}

static void
remove_ipset(ipset *i,unsigned at,unsigned count){
	if(i->rangecount > at + count){
		memmove(&i->ranges[at],&i->ranges[at + count],
			sizeof(*i->ranges) * (i->rangecount - (at + count)));
	}
	if((i->rangecount -= count) == 0){
		Free(i->ranges);
		i->ranges = NULL;
		i->maxranges = 0;
	}
}

static int
add_iprange(ipset *i,const iprange *merge){
	iprange *cur;
	unsigned z = 0;

	// loop over all portranges we cannot merge with due to their highest
	// port being less than our lowest - 1 (if equal, they absorb us).
	while(z != i->rangecount){
		cur = &i->ranges[z];
		if(merge->lower <= cur->upper + 1){
			break;
		}
		if(cur->upper == 0xffffffff){
			break;
		}
		++z;
	}

	// if z == i->rangecount, our lower ip is the highest in the list.
	if(z == i->rangecount){
		return append_ipset(i,merge);
	}
	
	// we know now cur->upper + 1 >= merge->lower.

	// we can't merge with this; our high element is too small.
	if(merge->upper + 1 < cur->lower && merge->upper != 0xffffffff){
		return insert_ipset(i,z,merge);
	}

	// if we enclose them on the lower end, grow them down
	if(merge->lower <= cur->lower - 1 && cur->lower){
		cur->lower = merge->lower;
	}

	// their lower is correct; we must see if we engulf them on the upper
	// end. if so, we may have to "eat" others, as well.
	if(merge->upper > cur->upper){
		iprange *tmp = cur;
		unsigned zin = ++z;

		cur->upper = merge->upper;
		// loop until we can't merge their lower end
		while(zin < i->rangecount){
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
			remove_ipset(i,z,zin - z);
		}
	}
	return 0;
}

static int
parse_ipranges(const char *buf,ipset *is){
	iprange ir;
	int ret,r;

	ret = 0;
	while((r = parse_ipv4range(buf + ret,&ir)) >= 0){
		if(add_iprange(is,&ir) < 0){
			free_ipset(is);
			return -1;
		}
		ret += r;
		if(buf[ret] == ','){
			++ret;
		}else{
			return ret;
		}
	}
	return -1;
}

static int
del_iprange(ipset *is,const iprange *cut){
	unsigned z = 0;

	while(z < is->rangecount){
		iprange *cur = &is->ranges[z];

		if(cut->lower > cur->upper || cut->upper < cur->lower){
			++z;
			continue;
		}
		if(cut->lower <= cur->lower){
			if(cur->upper > cut->upper){
				cur->lower = cut->upper + 1;
				break;
			}else{
				remove_ipset(is,z,1);
				continue;
			}
		}
		if(cut->upper >= cur->upper){
			cur->upper = cut->lower - 1;
			++z;
		}else{
			iprange ir;
			
			init_iprange(&ir,cur->lower,cut->lower - 1);
			if(insert_ipset(is,z,&ir)){
				return -1;
			}
			cur = &is->ranges[z + 1];
			cur->lower = cut->upper + 1;
			break;
		}
	}
	return 0;
}

void init_ipset(ipset *is){
	memset(is,0,sizeof(*is));
}

void free_ipset(ipset *is){
	if(is){
		Free(is->ranges);
		init_ipset(is);
	}
}

int ipset_is_singleton(const ipset *is,uint32_t *ip){
	if(is->rangecount != 1){
		return 0;
	}
	if(is->ranges[0].lower != is->ranges[0].upper){
		return 0;
	}
	*ip = is->ranges[0].lower;
	return 1;
}

int merge_ipsets(ipset *cur,const ipset *add){
	ipset curdup;
	unsigned z;

	if(clone_ipset(cur,&curdup)){
		return -1;
	}
	for(z = 0 ; z < add->rangecount ; ++z){
		iprange *ir = &add->ranges[z];

		if(add_iprange(&curdup,ir)){
			free_ipset(&curdup);
			return -1;
		}
	}
	free_ipset(cur);
	*cur = curdup;
	return 0;
}

int purge_ipsets(ipset *cur,const ipset *del){
	ipset curdup;
	unsigned z;

	if(clone_ipset(cur,&curdup)){
		return -1;
	}
	for(z = 0 ; z < del->rangecount ; ++z){
		const iprange *ir = &del->ranges[z];

		if(del_iprange(&curdup,ir)){
			free_ipset(&curdup);
			return -1;
		}
	}
	free_ipset(cur);
	*cur = curdup;
	return 0;
}

int iprange_clashes(const ipset *is,const iprange *ir){
	unsigned z;

	for(z = 0 ; z < is->rangecount ; ++z){
		const iprange *cur = &is->ranges[z];

		if(ir->upper <= cur->upper && ir->upper >= cur->lower){
			return 1;
		}else if(ir->lower <= cur->upper && ir->lower >= cur->lower){
			return 1;
		}else if(ir->lower <= cur->lower && ir->upper >= cur->upper){
			return 1;
		}
	}
	return 0;
}

int ipsets_equal(const ipset *i0,const ipset *i1){
	size_t s;

	if(i0->rangecount != i1->rangecount){
		return 0;
	}
	if(i0->rangecount == 0){
		return 1;
	}
	s = sizeof(*i0->ranges) * i0->rangecount;
	return !memcmp(i0->ranges,i1->ranges,s);
}

int ipsets_clash(const ipset *i0,const ipset *i1){
	unsigned z;

	for(z = 0 ; z < i1->rangecount ; ++z){
		iprange *cur = &i1->ranges[z];

		if(iprange_clashes(i0,cur)){
			return 1;
		}
	}
	return 0;
}

int ipset_encloses(const ipset *is,const ipset *ipsub){
	unsigned zsub;

	for(zsub = 0 ; zsub < ipsub->rangecount ; ++zsub){
		iprange *isub = &ipsub->ranges[zsub];
		unsigned z;

		for(z = 0 ; z < is->rangecount ; ++z){
			iprange *i = &is->ranges[z];

			if(ip_in_range(i,isub->lower)){
				if(ip_in_range(i,isub->upper)){
					break;
				}
				return 0;
			}
		}
		if(z == is->rangecount){
			return 0;
		}
	}
	return 1;
}

static int
postparse_ipset(ipset *addr,int invert){
	if(invert){
		ipset tmp;

		init_ipset(&tmp);
		if(add_iprange(&tmp,&IPRANGE_ALL)){
			free_ipset(addr);
			return -1;
		}
		if(purge_ipsets(&tmp,addr)){
			free_ipset(&tmp);
			free_ipset(addr);
			return -1;
		}
		free_ipset(addr);
		*addr = tmp;
	}
	if(addr->rangecount == 0){
		bitch("IPv4 set was empty\n");
		return -1;
	}
	return 0;
}

int parse_initialized_ipset(const char *text,ipset *is){
	int invert,i,inbracket = 0;
	const char *start = text;

	is->rangecount = 0;
	parse_whitespace(&text);
	
	// is the ! operator being used to invert the selection?
	invert = 0;
	if(*text == '!'){
		invert = 1;
		++text;
	}

	parse_whitespace(&text);

	// are we allowing a comma-separated list of ip ranges?
	if(*text == '['){
		inbracket = 1;
		++text;
	}
		
	// accept the any keyword
	if(strncasecmp(text,"any",3) == 0){
		if(add_iprange(is,&IPRANGE_ALL) < 0){
			return -1;
		}
		text += 3;
	}else{
		if((i = parse_ipranges(text,is)) <= 0){
			return -1;
		}
		text += i;
	}

	parse_whitespace(&text);

	if(inbracket){
		if(*text != ']'){
			bitch("Missing right bracket: %s\n",start);
			free_ipset(is);
			return -1;
		}
		++text;
	}
	
	// frees everything associated on error
	if(postparse_ipset(is,invert)){
		return -1;
	}
	return text - start;
}

int parse_ipset(const char *text,ipset *is){
	init_ipset(is);
	return parse_initialized_ipset(text,is);
}

int iprange_from_route(iprange *i,uint32_t dst,unsigned bits){
	uint32_t imask = 0xffffffff;

	if(bits > 32){
		bitch("%u bits provided for IPv4 route\n",bits);
		return -1;
	}
	bits = 32 - bits;
	while(bits--){
		imask <<= 1;
	}
	i->lower = dst & imask;
	i->upper = (dst & imask) | ~imask;
	return 0;
}

void swap_ipsets(ipset *i0,ipset *i1){
	iprange *ranges;
	unsigned rcount;

	ranges = i0->ranges;
	rcount = i0->rangecount;
	i0->ranges = i1->ranges;
	i0->rangecount = i1->rangecount;
	i1->ranges = ranges;
	i1->rangecount = rcount;
}

int clone_ipset(const ipset *src,ipset *dst){
	size_t heap;

	memset(dst,0,sizeof(*dst));
	if(src->rangecount > src->maxranges){
		bitch("Corruption detected: provided %u/%u ipset\n",
					src->rangecount,src->maxranges);
		return -1;
	}
	if( (heap = sizeof(*src->ranges) * src->rangecount) ){
		dst->ranges = Memdup("ipset ranges",src->ranges,heap);
		if(dst->ranges == NULL){
			return -1;
		}
		dst->rangecount = src->rangecount;
		dst->maxranges = src->maxranges;
	}
	return 0;
}

int ipset_cmp(const ipset *is0,const ipset *is1){
	unsigned z;

	for(z = 0 ; z < is0->rangecount && z < is1->rangecount ; ++z){
		int ret;

		if( (ret = is0->ranges[z].lower < is1->ranges[z].lower ? -1 :
			is0->ranges[z].lower > is1->ranges[z].lower ? 1 :
			is0->ranges[z].upper < is1->ranges[z].upper ? -1 :
			is0->ranges[z].upper > is1->ranges[z].upper) ){
			return ret;
		}
	}
	return is0->rangecount < is1->rangecount ? -1 :
		is0->rangecount > is1->rangecount;
}

// must be 64-bit to represent 2^32 + 1 values [0..2^32], not [0..2^32)
uint64_t ipset_area(const ipset *is){
	uint64_t area = 0;
	unsigned z;

	for(z = 0 ; z < is->rangecount ; ++z){
		area += (uint64_t)is->ranges[z].upper - is->ranges[z].lower + 1;
	}
	return area;
}
