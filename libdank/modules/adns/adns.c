#include <adns.h>
#include <stdlib.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/adns/adns.h>

static adns_state astate;

int init_asynchronous_dns(void){
	// FIXME disable diagnostics with adns_if_noerrprint and ands_if_noserverwarn
	adns_initflags flags = adns_if_nosigpipe | adns_if_noenv;

	if(adns_init(&astate,flags,NULL)){
		return -1;
	}
	return 0;
}

// FIXME no AAAA support in libadns?!
int synchronous_dnslookup(const char *query,struct in_addr *sina){
	adns_answer *aa = NULL;

	if(adns_synchronous(astate,query,adns_r_a,adns_qf_none,&aa)){
		bitch("Couldn't lookup %s\n",query);
		return -1;
	}
	if(aa == NULL){
		bitch("NULL lookup for %s\n",query);
		return -1;
	}
	memcpy(sina,aa->rrs.inaddr,sizeof(*sina));
	free(aa);
	return 0;
}

int stop_asynchronous_dns(void){
	return 0;
}
