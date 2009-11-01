#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libdank/utils/parse.h>
#include <libdank/objects/ipset.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

static inline int is_key_char(char c){
	return !isspace(c) && isgraph(c) && c!='=' && c!='"';
}

// all chars !\in isspace \in isgraph
char *parse_next_graph(char *buf,unsigned *len){
	char *toke;

	*len = 0;
	while(isspace(*buf)){
		++*len;
		++buf;
	}
	if(isgraph(*(toke = buf))){
		do{
			++*len;
		}while(!isspace(*++buf) && isgraph(*buf));
		return toke;
	}
	return NULL;
}

// return in network-byte order
int parse_ipv4address(const char *buf,uint32_t *s){
	const size_t INET_ADDRSTRMIN = 7;
	char ipbuf[INET_ADDRSTRLEN];
	const char *iptext;
	unsigned b = 0;	
	int ret = 0;

	while(isspace(*buf)){
		++buf;
		++ret;
	}
	iptext = buf;
	while(b + 1 < INET_ADDRSTRLEN && (isdigit(*buf) || *buf == '.')){
		++b;
		++buf;
	}
	ret += b;
	if(b >= INET_ADDRSTRLEN || b < INET_ADDRSTRMIN){
		bitch("Expected IPv4 address, got %s\n",iptext);
		return -1;
	}
	memcpy(ipbuf,iptext,b);
	ipbuf[b] = '\0';
	if(Inet_pton(AF_INET,ipbuf,s)){
		return -1;
	}
	return ret;
}

// return in host byte order
int parse_port(const char *buf,uint16_t *port,int silent){
	const char *cur = buf;
	unsigned long p = 0;

	while(isdigit(*cur)){
		p *= 10;
		p += *cur - '0';
		if(p > 65535){
			if(!silent){
				bitch("Wanted port, got %s\n",buf);
			}
			return -1;
		}
		++cur;
	}
	if(cur == buf){
		if(!silent){
			bitch("Wanted port, got %s\n",buf);
		}
		return -1;
	}
	*port = p;
	return cur - buf;
}

// return in host byte order
int parse_portrange(const char *buf,portrange *pr,int silent){
	const char *start = buf;
	int i;

	parse_whitespace(&buf);
	pr->lower = 0;
	pr->upper = 65535;
	if(*buf == ':' || *buf == '-'){
		++buf;
		if((i = parse_port(buf,&pr->upper,silent)) < 0){
			return -1;
		}
		buf += i;
	}else if((i = parse_port(buf,&pr->lower,silent)) < 0){
		return -1;
	}else{
		buf += i;
		if((*buf == '-' || *buf == ':') && isdigit(*(buf + 1))){
			++buf;
			if((i = parse_port(buf,&pr->upper,silent)) < 0){
				return -1;
			}
			buf += i;
			if(pr->lower > pr->upper){
				uint16_t tmp;

				tmp = pr->lower;
				pr->lower = pr->upper;
				pr->upper = tmp;
			}
		}else if(*buf == ':' || *buf == '-'){
			++buf;
		}else{
			pr->upper = pr->lower;
		}
	}
	if(isspace(*buf) || *buf == ',' || *buf == '\0' || *buf == '\"'){
		return buf - start;
	}
	return -1;
}
