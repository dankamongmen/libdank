#include <libdank/ersatz/compat.h>

#ifdef LIB_COMPAT_LINUX
#include <string.h>
#include <net/if.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/objustring.h>
#include <libdank/modules/netlink/ethtool.h>

/* These are taken from ethtool(8)'s ethtool-utils.h file; they're necessary to
 * compile against old glibc versions (such as that on RHEL3) with ethtool.h
 * headers broken against the included kernel's asm/types.h. FIXME */
typedef unsigned long long u64;
typedef __uint32_t u32;
typedef __uint16_t u16;
typedef __uint8_t u8;
/* End inclusions from ethtool-util.h FIXME */

int check_ethtool_support(const char *nic,char **drvstr){
	struct ethtool_drvinfo drvinfo = { .cmd = ETHTOOL_GDRVINFO, };
	ustring u = USTRING_INITIALIZER;
	struct ifreq ifr;
	int sd,ret;

	if(strlen(nic) >= sizeof(ifr.ifr_name)){
		bitch("NIC name (%s) too long (%zu >= %zu)\n",nic,strlen(nic),sizeof(ifr.ifr_name));
		return -1;
	}
	if((sd = Socket(AF_INET,SOCK_DGRAM,0)) < 0){
		reset_ustring(&u);
		return -1;
	}
	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,nic);
	ifr.ifr_data = (caddr_t)&drvinfo;
	if((ret = ioctl(sd,SIOCETHTOOL,&ifr)) == 0){
		if(printUString(&u,"%s %s %s %s",drvinfo.driver,drvinfo.version,drvinfo.fw_version,drvinfo.bus_info) < 0){
			reset_ustring(&u);
			ret = -1;
		}else{
			*drvstr = u.string;
		}
	}else{
		reset_ustring(&u);
	}
	ret |= Close(sd);
	return ret;
}

int get_nic_linkspeed(const char *nic){
	struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET, };
	int sd,iocret,speed = -1;
	struct ifreq ifr;

	if(strlen(nic) >= sizeof(ifr.ifr_name)){
		bitch("NIC name (%s) too long (%zu >= %zu)\n",nic,strlen(nic),sizeof(ifr.ifr_name));
		return -1;
	}
	if((sd = Socket(AF_INET,SOCK_DGRAM,0)) < 0){
		return -1;
	}
	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,nic);
	ifr.ifr_data = (caddr_t)&ecmd;
	if((iocret = ioctl(sd,SIOCETHTOOL,&ifr)) == 0){
		speed = ecmd.speed;
		#define SPEED_CHECK(s,mbps) (s) == SPEED_##mbps ? #mbps :
		nag("Link speed: %d %s\n",speed,
/*#ifdef SPEED_10000
			SPEED_CHECK(speed,10000)
#endif
#ifdef SPEED_2500
			SPEED_CHECK(speed,2500)
#endif*/
			SPEED_CHECK(speed,1000)
			SPEED_CHECK(speed,100)
			SPEED_CHECK(speed,10)
			"unknown link speed");
		#undef SPEED_CHECK
		// FIXME convert to bits from megabits?
	}else{
		moan("Couldn't apply ETHTOOL_GSET ioctl() to %d\n",sd);
	}
	speed |= Close(sd);
	return speed;
}

int get_nic_linkstatus(const char *nic){
	struct ethtool_value ethval = { .cmd = ETHTOOL_GLINK, };
	int sd,iocret,ret = -1;
	struct ifreq ifr;

	if(strlen(nic) >= sizeof(ifr.ifr_name)){
		bitch("NIC name (%s) too long (%zu >= %zu)\n",nic,strlen(nic),sizeof(ifr.ifr_name));
		return -1;
	}
	if((sd = Socket(AF_INET,SOCK_DGRAM,0)) < 0){
		return -1;
	}
	memset(&ifr,0,sizeof(ifr));
	strcpy(ifr.ifr_name,nic);
	ifr.ifr_data = (caddr_t)&ethval;
	if((iocret = ioctl(sd,SIOCETHTOOL,&ifr)) == 0){
		if( (ret = ethval.data) ){
			nag("Detected link status %d\n",ethval.data);
		}else{
			nag("No link detected\n");
		}
	}else{
		moan("Couldn't apply ETHTOOL_GLINK ioctl() to %d\n",sd);
	}
	Close(sd);
	return ret;
}
#endif
