#include <stdint.h>
#include <libdank/utils/rfc3330.h>

// We could likely do this faster with a lookup table or something...
rfc3330_type categorize_ipv4address(uint32_t ip){
	const struct {
		int (*predfxn)(uint32_t);
		rfc3330_type rettype;
	} testfxns[] = {
		{
			.predfxn = ipv4_is_rfc3330thisnetwork,
			.rettype = RFC3330_THISNETWORK,
		},{
			.predfxn = ipv4_is_rfc3330private,
			.rettype = RFC3330_PRIVATE,
		},{
			.predfxn = ipv4_is_rfc3330publicdatanet,
			.rettype = RFC3330_PUBLICDATANET,
		},{
			.predfxn = ipv4_is_rfc3330loopback,
			.rettype = RFC3330_LOOPBACK,
		},{
			.predfxn = ipv4_is_rfc3330linklocal,
			.rettype = RFC3330_LINKLOCAL,
		},{
			.predfxn = NULL,
			.rettype = RFC3330_UNCLASSIFIED,
		}
	}, *curtest;

	for(curtest = testfxns ; curtest->predfxn ; ++curtest){
		if(curtest->predfxn(ip)){
			break;
		}
	}
	return curtest->rettype;

}
