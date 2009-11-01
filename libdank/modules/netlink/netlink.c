#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <libdank/utils/mac.h>
#include <libdank/utils/string.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/ipset.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/netlink/netlink.h>
#include <libdank/modules/netlink/ethtool.h>

typedef struct route {
	struct route *next;
	// FIXME need support for variable-length addresses!!!
	// host byte-order destination scope of and source hint for route
	uint32_t dst,src;
	// bits valid for destination scope, table of route, type of route
	unsigned bits,table,type;
} route;

typedef struct neighbor {
	struct neighbor *next;
	unsigned state,flags,type,family;
	size_t lllen,netlen;
	unsigned char *netdst,*lldst;
} neighbor;

typedef struct nic {
	struct nic *next;
	int idx;
	char *name,*drvinfo;
	unsigned mtu;
	int linktype,txqlen;
	route *routes;
	neighbor *neighbors;
	size_t addrlen;
	// Yes, a NIC can have multiple addresses -- but it can have only one
	// default source address (used when there's no route src hint). Each
	// route has its particular source hint associated with it.
	uint32_t srcaddr;
} nic;

typedef struct netlink_state {
	int sd;
	nic *nics;
	unsigned seq;
} netlink_state;

static netlink_state singleton_state = {
	.sd = -1,
};

static nic *nics;
static pthread_mutex_t netlink_lock = PTHREAD_MUTEX_INITIALIZER;

static void
free_neigh(neighbor *n){
	if(n){
		Free(n->netdst);
		Free(n->lldst);
		Free(n);
	}
}

static void
free_route(route *r){
	if(r){
		Free(r);
	}
}

static void
free_nic(nic *n){
	if(n){
		typeof(*n->neighbors) *nb;
		typeof(*n->routes) *r;

		while(n->neighbors){
			nb = n->neighbors->next;
			free_neigh(n->neighbors);
			n->neighbors = nb;
		}
		while(n->routes){
			r = n->routes->next;
			free_route(n->routes);
			n->routes = r;
		}
		Free(n->drvinfo);
		Free(n->name);
		Free(n);
	}
}

unsigned get_maximum_mtu(void){
	const typeof(*nics) *n;
	unsigned maxmtu = 0;

	pthread_mutex_lock(&netlink_lock);
	for(n = nics ; n ; n = n->next){
		if(n->mtu > maxmtu){
			maxmtu = n->mtu;
		}
	}
	pthread_mutex_unlock(&netlink_lock);
	return maxmtu;
}

static int
stop_netlink_state(netlink_state *nlstate){
	typeof (*nics) *n;
	int ret = 0;

	pthread_mutex_lock(&netlink_lock);
	if(nlstate->sd >= 0){
		ret |= Close(nlstate->sd);
		nlstate->sd = -1;
	}
	while( (n = nics) ){
		nics = nics->next;
		free_nic(n);
	}
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

int stop_netlink_layer(void){
	return stop_netlink_state(&singleton_state);
}

#ifdef LIB_COMPAT_LINUX
#include <asm/types.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/netdevice.h>

static neighbor *
create_neigh(unsigned type,unsigned state,unsigned flags,
		const void *lldst,size_t lllen,
		const void *dst,size_t dstlen,
		unsigned family){
	neighbor *ret;

	if( (ret = Malloc("neighbor",sizeof(*ret))) ){
		ret->type = type;
		ret->flags = flags;
		ret->state = state;
		ret->family = family;
		if( (ret->lllen = lllen) ){
			if((ret->lldst = Memdup("linkaddr",lldst,lllen)) == NULL){
				free_neigh(ret);
				return NULL;
			}
		}else{
			ret->lldst = NULL;
		}
		if( (ret->netlen = dstlen) ){
			if((ret->netdst = Memdup("netaddr",dst,dstlen)) == NULL){
				free_neigh(ret);
				return NULL;
			}
		}else{
			ret->netdst = NULL;
		}
	}
	return ret;
}

static route *
create_route(uint32_t dst,unsigned dbits,uint32_t src,unsigned table,unsigned type){
	route *r;

	if( (r = Malloc("route",sizeof(*r))) ){
		r->bits = dbits;
		r->dst = dst;
		r->src = src;
		r->table = table;
		r->type = type;
	}
	return r;
}

static nic *
create_nic(int linktype,int idx,const char *name,char *drvinfo,unsigned mtu,int txqlen){
	nic *ret;

	if( (ret = Malloc("NIC",sizeof(*ret))) ){
		if((ret->name = Strdup(name)) == NULL){
			Free(ret);
			return NULL;
		}
		// FIXME for variable-length netaddr
		ret->addrlen = sizeof(ret->srcaddr);
		ret->drvinfo = drvinfo;
		ret->idx = idx;
		ret->linktype = linktype;
		ret->mtu = mtu;
		ret->txqlen = txqlen;
		ret->neighbors = NULL;
		ret->routes = NULL;
	}
	return ret;
}

static int
add_nic_locked(int idx,int itype,const char *name,char *drvinfo,
				unsigned mtu,int txqlen){
	typeof(*nics) *n;

	for(n = nics ; n ; n = n->next){
		if(n->idx == idx){
			bitch("NIC %s already existed at index %d\n",n->name,n->idx);
			return -1;
		}
	}
	nag("Got new link %d (type %d): %s\n",idx,itype,name);
	if((n = create_nic(itype,idx,name,drvinfo,mtu,txqlen)) == NULL){
		return -1;
	}
	n->next = nics;
	nics = n;
	return 0;
}

static int
add_neigh_locked(int idx,unsigned type,unsigned state,unsigned flags,
		const void *lldst,size_t lllen,const void *dst,size_t dstlen,
		unsigned family){
	typeof(*nics) *nnic;
	neighbor *neigh;

	for(nnic = nics ; nnic ; nnic = nnic->next){
		if(nnic->idx == idx){
			break;
		}
	}
	if(nnic == NULL){
		bitch("Neighbor referenced unknown NIC\n");
		return -1;
	}
	nag("Got new neighbor on NIC %s\n",nnic->name);
	if((neigh = create_neigh(type,state,flags,lldst,lllen,dst,dstlen,family)) == NULL){
		return -1;
	}
	neigh->next = nnic->neighbors;
	nnic->neighbors = neigh;
	return 0;
}

static int
add_neigh(int idx,unsigned type,unsigned state,unsigned flags,
		const void *lldst,size_t lllen,const void *dst,size_t dstlen,
		unsigned family){
	int ret;

	pthread_mutex_lock(&netlink_lock);
	ret = add_neigh_locked(idx,type,state,flags,lldst,lllen,dst,dstlen,family);
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

static int
add_route_locked(const struct rtmsg *r,const void *dst,const void *src,int iif){
	typeof(*nics) *n;

	nag("af %d tbl %u scope %u type %u bits %u iif %u\n",r->rtm_family,r->rtm_table,r->rtm_scope,r->rtm_type,r->rtm_dst_len,iif);
	for(n = nics ; n ; n = n->next){
		if(n->idx == iif){
			uint32_t dip = 0,sip = 0;
			route *rt;

			if(dst){
				memcpy(&dip,dst,(r->rtm_dst_len + (CHAR_BIT - 1)) / CHAR_BIT);
				dip = ntohl(dip);
			}
			if(src){
				memcpy(&sip,src,(r->rtm_src_len + (CHAR_BIT - 1)) / CHAR_BIT);
				sip = ntohl(sip);
			}
			if((rt = create_route(dip,r->rtm_dst_len,sip,
					r->rtm_table,r->rtm_type)) == NULL){
				return -1;
			}
			rt->next = n->routes;
			n->routes = rt;
			if(n->srcaddr == 0){
				if((n->srcaddr = sip) == 0 && r->rtm_type == RTN_LOCAL){
					n->srcaddr = dip;
				}
			}
			return 0;
		}
	}
	bitch("Unknown interface index: %u\n",iif);
	return -1;
}

static int
add_route(const struct rtmsg *r,const void *dst,const void *src,unsigned iif){
	int ret;

	pthread_mutex_lock(&netlink_lock);
	ret = add_route_locked(r,dst,src,iif);
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

static int
send_getlink_msg(netlink_state *nlstate){
	struct req {
		struct nlmsghdr nh;
		struct rtmsg rtmsg;
	} req = {
		.nh = {
			.nlmsg_seq = ++nlstate->seq,
			.nlmsg_len = NLMSG_LENGTH(sizeof(req.rtmsg)),
			.nlmsg_type = RTM_GETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT,
			.nlmsg_pid = getpid(),
		},
		.rtmsg = {
			.rtm_family = AF_UNSPEC,
			.rtm_protocol = RTPROT_UNSPEC,
			.rtm_type = RTN_UNSPEC,
		},
	};
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov[] = { { &req.nh, req.nh.nlmsg_len }, };
	struct msghdr msg = { &sa, sizeof(sa), iov, sizeof(iov) / sizeof(*iov), NULL, 0, 0 };

	if(Sendmsg(nlstate->sd,&msg,0) <= 0){
		return -1;
	}
	return 0;
}

static int
send_getneigh_msg(netlink_state *nlstate){
	struct req {
		struct nlmsghdr nh;
		struct ndmsg nmsg;
	} req = {
		.nh = {
			.nlmsg_seq = ++nlstate->seq,
			.nlmsg_len = NLMSG_LENGTH(sizeof(req.nmsg)),
			.nlmsg_type = RTM_GETNEIGH,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT,
			.nlmsg_pid = getpid(),
		},
	};
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};
	struct iovec iov[] = { { &req.nh, req.nh.nlmsg_len }, };
	struct msghdr msg = { &sa, sizeof(sa), iov, sizeof(iov) / sizeof(*iov), NULL, 0, 0 };

	if(Sendmsg(nlstate->sd,&msg,0) <= 0){
		return -1;
	}
	return 0;
}

static int
send_getroute_msg(netlink_state *nlstate){
	struct req {
		struct nlmsghdr nh;
		struct rtgenmsg rtmsg;
	} req = {
		.nh = {
			.nlmsg_seq = ++nlstate->seq,
			.nlmsg_len = sizeof(req),
			.nlmsg_type = RTM_GETROUTE,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT,
			.nlmsg_pid = getpid(),
		},
		.rtmsg = {
			.rtgen_family = AF_INET,
		},
	};
	// FIXME concession to -Wstrict-aliasing=2 with gcc
	const struct sockaddr/*_nl*/ sa = {
		.sa_family = AF_NETLINK,
	};

	if(Sendto(nlstate->sd,&req,sizeof(req),0,&sa,sizeof(sa)) <= 0){
		return -1;
	}
	return 0;
}

static int
parse_rtattr(struct rtattr *tb[],int sentinel,struct rtattr *rta,size_t len){
	memset(tb,0,sizeof(*tb) * sentinel);
	while(RTA_OK(rta,len)){
		if(rta->rta_type < sentinel){
			// nag("Got rtattr index %d\n",rta->rta_type);
			tb[rta->rta_type] = rta;
		}else{
			nag("Unknown rtattr index %d >= %d\n",rta->rta_type,sentinel);
		}
		rta = RTA_NEXT(rta,len);
	}
	if(len){
		bitch("Exceeded rtattr payload by %zu\n",len);
		return -1;
	}
	return 0;
}

// RTA_MAX is the actual maximum index that can be returned, not a sentinel!
// RTA_SESSION is defined in RedHat Enterprise Linux 3 kernels and perhaps
// others as rtattr 13. We really need to compile against the kernel's
// rtnetlink.h, not glibc's FIXME. Until then, consider RTA_MAX off by one
// (we can't redefine it, as it's calculated based off __RTA_MAX which is the
// terminating member of the enum). Bleh!
static int
decode_getroute_msg(struct nlmsghdr *nlh,size_t len,pid_t pid){
	int parts = 0;

	while(NLMSG_OK(nlh,len)){
		struct rtattr *tb[RTA_MAX + 1]; // see leading comment
		struct rtmsg *r;

		if(nlh->nlmsg_flags & ~NLM_F_MULTI){
			bitch("Unexpected flags: %u\n",nlh->nlmsg_flags);
			return -1;
		}
		if(nlh->nlmsg_type == NLMSG_DONE){
			// nag("decoded netlink message\n");
			return 0;
		}else if(nlh->nlmsg_type != RTM_NEWROUTE){
			bitch("Netlink message was wrong type (wanted %u, got %hu)\n",
					RTM_NEWROUTE,nlh->nlmsg_type);
			return -1;
		}
		++parts;
		// FIXME check sequence number (nlmsg_seq). also, pid might not
		// be process PID if there's multiple sockets -- store in nthl!
		if((pid_t)nlh->nlmsg_pid != pid){
			bitch("Misaddressed to PID %d\n",nlh->nlmsg_pid);
			return -1;
		}
		// nag("%zu payload bytes for %zu\n",RTM_PAYLOAD(nlh),NLMSG_LENGTH(sizeof(*r)));
		r = NLMSG_DATA(nlh);
		if(parse_rtattr(tb,sizeof(tb) / sizeof(*tb),RTM_RTA(r),len - NLMSG_LENGTH(sizeof(*r)))){
			return -1;
		}
		// rtm_dst_len == 0 allows NULL RTA_DST (it's a default route)
		if(r->rtm_dst_len && tb[RTA_DST] == NULL){
			bitch("Malformed RTM_NEWROUTE message (no destination)\n");
			return -1;
		}
		if(tb[RTA_OIF] == NULL){
			bitch("Malformed RTM_NEWROUTE message (no interface)\n");
			return -1;
		}
		if(add_route(r,tb[RTA_DST] ? RTA_DATA(tb[RTA_DST]) : NULL,
				tb[RTA_SRC] ? RTA_DATA(tb[RTA_SRC]) : NULL,
				*(int *)RTA_DATA(tb[RTA_OIF]))){
			return -1;
		}
		nlh = NLMSG_NEXT(nlh,len);
	}
	if(len){
		bitch("Exceeded netlink message by %zu\n",len);
		return -1;
	}
	return parts;
}

static int
get_idx_txqlen(const char *name){
	struct ifreq ifr;
	int sd = -1;

	if(strlen(name) >= sizeof(ifr.ifr_name)){
		bitch("Invalid interface name: %s\n",name);
		goto err;
	}
	if((sd = Socket(AF_INET,SOCK_STREAM,0)) < 0){
		goto err;
	}
	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,name);
	if(ioctl(sd,SIOCGIFTXQLEN,&ifr)){
		moan("Couldn't read TX queue length for %s\n",name);
		goto err;
	}
	if(ifr.ifr_qlen < 0){
		bitch("Bogus TX queue length for %s: %d\n",name,ifr.ifr_qlen);
		goto err;
	}
	if(Close(sd)){
		return -1;
	}
	return ifr.ifr_qlen;

err:
	if(sd >= 0){
		Close(sd);
	}
	return -1;
}

static int
add_nic(const struct ifinfomsg *i,const char *name,unsigned mtu){
	char *drvstr = NULL;
	int ret,txqlen;

	if((txqlen = get_idx_txqlen(name)) < 0){
		return -1;
	}
	if(i->ifi_type == ARPHRD_ETHER && check_ethtool_support(name,&drvstr) == 0){
		nag("Found ethtool support: %s\n",drvstr);
	}else{
		nag("No ethtool support for %s (type %u)\n",name,i->ifi_type);
	}
	pthread_mutex_lock(&netlink_lock);
	ret = add_nic_locked(i->ifi_index,i->ifi_type,name,drvstr,mtu,txqlen);
	if(ret){
		Free(drvstr);
	}
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

#ifndef NDA_RTA
#define NDA_RTA(r) ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif
#ifndef NDA_PAYLOAD
#define NDA_PAYLOAD(n)  NLMSG_PAYLOAD(n,sizeof(struct ndmsg))
#endif

static int
decode_getlink_msg(struct nlmsghdr *nlh,size_t len,pid_t pid){
	int parts = 0;

	while(NLMSG_OK(nlh,len)){
		struct rtattr *tb[IFLA_MAX + 1];
		struct ifinfomsg *i;

		if(nlh->nlmsg_flags & ~NLM_F_MULTI){
			bitch("Unexpected flags: %u\n",nlh->nlmsg_flags);
			return -1;
		}
		if(nlh->nlmsg_type == NLMSG_DONE){
			return 0;
		}else if(nlh->nlmsg_type != RTM_NEWLINK){
			bitch("Netlink message was wrong type (wanted %u, got %hu)\n",
					RTM_NEWLINK,nlh->nlmsg_type);
			return -1;
		}
		// FIXME check sequence number (nlmsg_seq). also, pid might not
		// be process PID if there's multiple sockets -- store in nthl!
		++parts;
		if((pid_t)nlh->nlmsg_pid != pid){
			bitch("Misaddressed to PID %d\n",nlh->nlmsg_pid);
			return -1;
		}
		// nag("%zu payload bytes for %zu\n",IFLA_PAYLOAD(nlh),NLMSG_LENGTH(sizeof(i)));
		i = NLMSG_DATA(nlh);
		if(parse_rtattr(tb,sizeof(tb) / sizeof(*tb),IFLA_RTA(i),len - NLMSG_LENGTH(sizeof(*i)))){
			return -1;
		}
		if(tb[IFLA_IFNAME] == NULL){
			bitch("Malformed RTM_NEWLINK message (no iface name)\n");
			return -1;
		}
		if(tb[IFLA_MTU] == NULL){
			bitch("Malformed RTM_NEWLINK message (no MTU)\n");
			return -1;
		}
		if(add_nic(i,(const char *)RTA_DATA(tb[IFLA_IFNAME]),*(unsigned *)RTA_DATA(tb[IFLA_MTU]))){
			return -1;
		}
		nlh = NLMSG_NEXT(nlh,len);
	}
	if(len){
		bitch("Exceeded netlink message by %zu\n",len);
		return -1;
	}
	return parts;
}

static int
decode_getneigh_msg(struct nlmsghdr *nlh,size_t len,pid_t pid){
	int parts = 0;

	while(NLMSG_OK(nlh,len)){
		struct rtattr *tb[IFLA_MAX + 1];
		struct ndmsg *n;

		if(nlh->nlmsg_flags & ~NLM_F_MULTI){
			bitch("Unexpected flags: %u\n",nlh->nlmsg_flags);
			return -1;
		}
		if(nlh->nlmsg_type == NLMSG_DONE){
			return 0;
		}else if(nlh->nlmsg_type != RTM_NEWNEIGH){
			bitch("Netlink message was wrong type (wanted %u, got %hu)\n",
					RTM_NEWNEIGH,nlh->nlmsg_type);
			return -1;
		}
		// FIXME check sequence number (nlmsg_seq). also, pid might not
		// be process PID if there's multiple sockets -- store in nthl!
		++parts;
		if((pid_t)nlh->nlmsg_pid != pid){
			bitch("Misaddressed to PID %d\n",nlh->nlmsg_pid);
			return -1;
		}
		// nag("%zu payload bytes for %zu\n",NDA_PAYLOAD(nlh),NLMSG_LENGTH(sizeof(i)));
		n = NLMSG_DATA(nlh);
		nag("Fam: %hu ifi: %d state: %hu flags: %hu\n",n->ndm_family,
				n->ndm_ifindex,n->ndm_state,n->ndm_flags);
		if(parse_rtattr(tb,sizeof(tb) / sizeof(*tb),NDA_RTA(n),len - NLMSG_LENGTH(sizeof(*n)))){
			return -1;
		}
		if(tb[NDA_LLADDR] == NULL){
			nag("Incomplete RTM_NEWNEIGH message (no link addr)\n");
		}
		if(tb[NDA_DST] == NULL){
			bitch("Malformed RTM_NEWNEIGH message (no dest)\n");
			return -1;
		}
		if(add_neigh(n->ndm_ifindex,n->ndm_type,n->ndm_state,n->ndm_flags,
			 tb[NDA_LLADDR] ? RTA_DATA(tb[NDA_LLADDR]) : NULL,
			 tb[NDA_LLADDR] ? RTA_PAYLOAD(tb[NDA_LLADDR]) : 0,
			 RTA_DATA(tb[NDA_DST]),RTA_PAYLOAD(tb[NDA_DST]),
			 n->ndm_family)){
			return -1;
		}
		nlh = NLMSG_NEXT(nlh,len);
		nag("Parsed a neighbor\n"); // FIXME store them
	}
	if(len){
		bitch("Exceeded netlink message by %zu\n",len);
		return -1;
	}
	return parts;
}

// FIXME wtf? workaround for VPS:
// Linux vps.qemfd.net 2.6.9-023stab048.6-smp #1 SMP Mon Nov 17 18:41:14 MSK 2008 i686 GNU/Linux
// 64-bit virutozzo container running 32b etch kernel and 32b lenny userspace
// aieeeeeeeeee
#ifndef MSG_CMSG_COMPAT
#define MSG_CMSG_COMPAT 0x80000000
#endif

static int
recv_getlink_msg(netlink_state *nlstate,void *buf,size_t buflen){
	struct iovec iov[] = {
		{	.iov_base = buf,	.iov_len = buflen,	},
	};
	struct sockaddr_nl sa;
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = iov,
		.msg_iovlen = sizeof(iov) / sizeof(*iov),
	};
	int mlen,ret;

	do{
		if((mlen = Recvmsg(nlstate->sd,&msg,0)) <= 0){
			return -1;
		}
		nag("Got %db from nl\n",mlen);
		if(sa.nl_pid != 0 || sa.nl_family != AF_NETLINK){
			bitch("Sender address was corrupted (%u/%d)\n",sa.nl_pid,sa.nl_family);
			return -1;
		}
		if(msg.msg_flags & ~MSG_CMSG_COMPAT){
			bitch("Unexpected flags: 0x%x\n",msg.msg_flags);
			return -1;
		}
	}while((ret = decode_getlink_msg(buf,mlen,getpid())) > 0);
	return ret;
}

static int
recv_getneigh_msg(netlink_state *nlstate,void *buf,size_t buflen){
	struct iovec iov[] = {
		{	.iov_base = buf,	.iov_len = buflen,	},
	};
	struct sockaddr_nl sa;
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = iov,
		.msg_iovlen = sizeof(iov) / sizeof(*iov),
	};
	int mlen,ret;

	do{
		if((mlen = Recvmsg(nlstate->sd,&msg,0)) <= 0){
			return -1;
		}
		nag("Got %db from nl\n",mlen);
		if(sa.nl_pid != 0 || sa.nl_family != AF_NETLINK){
			bitch("Sender address was corrupted (%u/%d)\n",sa.nl_pid,sa.nl_family);
			return -1;
		}
		if(msg.msg_flags & ~MSG_CMSG_COMPAT){
			bitch("Unexpected flags: 0x%x\n",msg.msg_flags);
			return -1;
		}
	}while((ret = decode_getneigh_msg(buf,mlen,getpid())) > 0);
	return ret;
}

static int
recv_getroute_msg(netlink_state *nlstate,void *buf,size_t buflen){
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = buflen,
	};
	struct sockaddr_nl sa;
	struct msghdr msg = {
		.msg_name = &sa,
		.msg_namelen = sizeof(sa),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int mlen,ret;

	do{
		if((mlen = Recvmsg(nlstate->sd,&msg,0)) <= 0){
			return -1;
		}
		nag("Got %db from nl\n",mlen);
		if(sa.nl_pid != 0 || sa.nl_family != AF_NETLINK){
			bitch("Sender address was corrupted (%u/%d)\n",sa.nl_pid,sa.nl_family);
			return -1;
		}
		if(msg.msg_flags & ~MSG_CMSG_COMPAT){
			bitch("Unexpected flags: 0x%x\n",msg.msg_flags);
			return -1;
		}
	}while((ret = decode_getroute_msg(buf,mlen,getpid())) > 0);
	return ret;
}

static int
init_netlink_state(netlink_state *nlstate){
	size_t buflen = BUFSIZ;
	char *buf = NULL;
	
	if((buf = Malloc("libnetlink buffer",buflen)) == NULL){
		goto err;
	}
	if((nlstate->sd = Socket(PF_NETLINK,SOCK_DGRAM,NETLINK_ROUTE)) < 0){
		goto err;
	}
	nlstate->seq = time(NULL);
	nag("Discovering links...\n");
	if(send_getlink_msg(nlstate)){
		goto err;
	}
	if(recv_getlink_msg(nlstate,buf,buflen)){
		goto err;
	}
	nag("Discovering neighbors...\n");
	if(send_getneigh_msg(nlstate)){
		goto err;
	}
	if(recv_getneigh_msg(nlstate,buf,buflen)){
		goto err;
	}
	nag("Discovering routes...\n");
	if(send_getroute_msg(nlstate)){
		goto err;
	}
	if(recv_getroute_msg(nlstate,buf,buflen)){
		goto err;
	}
	Free(buf);
	return 0;

err:
	stop_netlink_state(nlstate);
	Free(buf);
	return -1;
}

int init_netlink_layer(void){
	return init_netlink_state(&singleton_state);
}

static int
stringize_neigh(ustring *u,const neighbor *n,int linktype){
	char netstr[INET6_ADDRSTRLEN];

	if(!n->netlen || !inet_ntop(n->family,n->netdst,netstr,sizeof(netstr))){
		memset(netstr,0,sizeof(netstr));
	}
	if(linktype == ARPHRD_ETHER){
		char llbuf[ETH_ADDRSTRLEN] = "unknown";

		if(n->lllen == ETH_ALEN){
			if(mactoascii(n->lldst,llbuf,':',MAC_STD) == NULL){
				return -1;
			}
		}
		if(printUString(u," [neigh] %s ltype %d %s%sfam %u type %u state %u flags %u\n",
				llbuf,linktype,netstr,*netstr ? " " : "",
				n->family,n->type,n->state,n->flags) < 0){
			return -1;
		}
	}else if(n->lllen == 0){
		if(printUString(u," [neigh] p2p ltype %d %s%sfam %u type %u state %u flags %u\n",
				linktype,netstr,*netstr ? " " : "",
				n->family,n->type,n->state,n->flags) < 0){
			return -1;
		}
	}else{
		if(printUString(u," [neigh] ltype %d %s%sfam %u type %u state %u flags %u\n",
				linktype,netstr,*netstr ? " " : "",
				n->family,n->type,n->state,n->flags) < 0){
			return -1;
		}
	}
	return 0;
}

static int
stringize_nic(ustring *u,const nic *n){
	const typeof(*n->neighbors) *nb;
	const typeof(*n->routes) *r;

	if(printUString(u,"[nic] %s (%s) type %d mtu %u txqlen %d\n",n->name,
				n->drvinfo ? n->drvinfo : "unknown",
				n->linktype,n->mtu,n->txqlen) < 0){
		return -1;
	}
	for(r = n->routes ; r ; r = r->next){
		char dip[INET_ADDRSTRLEN];
		uint32_t dst = htonl(r->dst);

		inet_ntop(AF_INET,&dst,dip,sizeof(dip));
		if(printUString(u," [route] %s/%u type %u tbl %u\n",dip,r->bits,r->type,r->table) < 0){
			return -1;
		}
	}
	for(nb = n->neighbors ; nb ; nb = nb->next){
		if(stringize_neigh(u,nb,n->linktype)){
			return -1;
		}
	}
	return 0;
}

static int
stringize_netlink_locked(ustring *u,const netlink_state *ns){
	const typeof(*nics) *n;
	int ncount = 0;

	if(ns->sd < 0){
		bitch("Netlink layer is unitialized\n");
		return -1;
	}
	if(printUString(u,"netlink] sd %d\n",ns->sd) < 0){
		return -1;
	}
	for(n = nics ; n ; n = n->next){
		if(stringize_nic(u,n)){
			return -1;
		}
		++ncount;
	}
	if(printUString(u,"%d NICs\n",ncount) < 0){
		return -1;
	}
	return 0;
}

int stringize_netinfo(ustring *u){
	int ret;

	pthread_mutex_lock(&netlink_lock);
	ret = stringize_netlink_locked(u,&singleton_state);
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

// FIXME We need to either
// a) connect(2) to each target, and get the source address that way,
// 	(tight racey loud failure, need do connect(2) and getlocalname(2))
// b) do a routing lookup for each target
// 	(tight racey silent failure, need do lookup via rtnetlink(7))
// c) emulate longest-prefix match selection by maintaining a prefix trie
//	(loose racey silent failure, need maintain trie)
// d) pick the default route's source ip or do a skip-match
// 	(not even a chance at being correct for all cases)
//
// currently, this function is just plain broken; it doesn't detect multiple
// route cases, instead merely selecting any route which encloses the ipset.
//
// consult:
// http://linux-ip.net/html/routing-saddr-selection.html
// http://linux-ip.net/html/routing-selection.html
int srcroute_to_ipset(const ipset *i,uint32_t *src){
	typeof(*nics) *n;
	int ret = -1;

	pthread_mutex_lock(&netlink_lock);
	for(n = nics ; n ; n = n->next){
		typeof(*n->routes) *r;

		for(r = n->routes ; r ; r = r->next){
			iprange ir;
			ipset is = {
				.ranges = &ir,
				.rangecount = 1,
				.maxranges = 1,
			};

			if(iprange_from_route(&ir,r->dst,r->bits)){
				goto done;
			}
			if(ipset_encloses(&is,i)){
				if((*src = r->src) == 0){
					*src = n->srcaddr;
				}
				ret = 0;
				goto done;
			}
		}
	}
done:
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

static int
ip_is_local_locked(uint32_t ip){
	const typeof(*nics) *n;

	for(n = nics ; n ; n = n->next){
		const typeof(*nics->routes) *r;

		for(r = n->routes ; r ; r = r->next){
			if(r->table == RT_TABLE_LOCAL){
				iprange ir;

				if(iprange_from_route(&ir,r->dst,r->bits) == 0){
					if(ip_in_range(&ir,ip)){
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

int ip_is_local(uint32_t ip){
	int ret;

	pthread_mutex_lock(&netlink_lock);
	ret = ip_is_local_locked(ip);
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}

// FIXME this is horribly slow! We should be using a prefix trie.
// FIXME we should work with generic struct sockaddr objects -- all the pieces
// 	are here (dynamic lengths, etc), we just need per-AF downcalls
int setup_sockaddr_ll(const struct sockaddr_in *sina,struct sockaddr_ll *sll){
	uint32_t ip = ntohl(sina->sin_addr.s_addr);
	typeof(*nics) *n;
	int ret = -1;

	pthread_mutex_lock(&netlink_lock);
	for(n = nics ; n ; n = n->next){
		typeof(*n->neighbors) *nb;

		for(nb = n->neighbors ; nb ; nb = nb->next){
			if(nb->netlen != sizeof(ip)){
				continue;
			}
			if(memcmp(&nb->netdst,&sina->sin_addr.s_addr,nb->netlen)){
				continue;
			}
			if(nb->lllen == 0 || nb->lllen > sizeof(sll->sll_addr)){
				bitch("Neighbor entry cannot be used\n");
				goto done;
			}
			memset(sll,0,sizeof(*sll));
			sll->sll_family = AF_PACKET;
			sll->sll_ifindex = n->idx;
			sll->sll_halen = nb->lllen;
			memcpy(sll->sll_addr,nb->lldst,nb->lllen);
			ret = 0;
			goto done;
		}
	}
done:
	pthread_mutex_unlock(&netlink_lock);
	return ret;
}
#else
	int init_netlink_layer(void){
		return -1;
	}

	int stringize_netinfo(ustring *u __attribute__ ((unused))){
		return -1;
	}

	int srcroute_to_ipset(const ipset *i __attribute__ ((unused)),
				uint32_t *src __attribute__ ((unused))){
		return -1;
	}

	int ip_is_local(uint32_t ip __attribute__ ((unused))){
		return -1;
	}

	int setup_sockaddr_ll(const struct sockaddr_in *sina __attribute__ ((unused)),
				struct sockaddr_ll *sll __attribute__ ((unused))){
		return -1;
	}
#endif
