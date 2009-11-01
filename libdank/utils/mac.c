#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <libdank/utils/hex.h>
#include <libdank/utils/mac.h>
#include <libdank/ersatz/compat.h>
#include <libdank/objects/logctx.h>

int maccmp(const unsigned char *m1,const unsigned char *m2){
	return memcmp(m1,m2,ETHER_ADDR_LEN);
}

char *mactoascii(const unsigned char * restrict mac,char * restrict buf,
				int sep,mac_format_e type){
	if(sep == EOF){
		sprintf(buf,"%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
				mac[0],mac[1],mac[2], mac[3],mac[4],mac[5]);
		return buf;
	}
	switch(type){
		case MAC_CISCO:
			sprintf(buf,"%2.2X%2.2X%c%2.2X%2.2X%c%2.2X%2.2X",
					mac[0],mac[1],sep,
					mac[2],mac[3],sep,
					mac[4],mac[5]);
			break;
		case MAC_HP:
			sprintf(buf,"%2.2X%2.2X%2.2X%c%2.2X%2.2X%2.2X",
					mac[0],mac[1],mac[2],sep,
					mac[3],mac[4],mac[5]);
			break;
		case MAC_STD:
			sprintf(buf,"%2.2X%c%2.2X%c%2.2X%c%2.2X%c%2.2X%c%2.2X",
					mac[0],sep,mac[1],sep,mac[2],sep,
					mac[3],sep,mac[4],sep,mac[5]);
			break;
		default:
			bitch("Unknown encoding: %d\n",type);
			return NULL;
	}
	return buf;
}

int asciitomac(const char * restrict buf,unsigned char * restrict mac,int sep){
	int ret = 0;

	while(isspace(*buf)){
		++buf;
		++ret;
	}
	if(hextoascii(buf,mac,sep,ETH_ALEN)){
		bitch("Expected MAC address, got %s\n",buf);
		return -1;
	}
	buf += ETH_ADDRSTRLEN - 1;
	if(*buf != '\0' && *buf != ' ' && *buf != '\t' && *buf !=','){
		bitch("Expected MAC address, got %s\n",buf);
		return -1;
	}
	ret += ETH_ADDRSTRLEN;
	return ret;
}

int uprintf_mac(ustring *u,const unsigned char *mac,int sep){
	char buf[ETH_ADDRSTRLEN];

	mactoascii(mac,buf,sep,MAC_STD);
	return (printUString(u,"%s",buf) < 0) ? -1 : 0;
}

int xmlize_mac(ustring *u,const unsigned char *mac){
	char buf[ETH_ADDRSTRLEN];

	mactoascii(mac,buf,':',MAC_STD);
	return (printUString(u,"<mac>%s</mac>",buf) < 0) ? -1 : 0;
}
