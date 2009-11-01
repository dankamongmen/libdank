#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <libdank/utils/hex.h>
#include <libdank/objects/logctx.h>

// Decode a string with precisely len pairs of hex digits, seperated by sep
// (which may be EOF to indicate no seperator), into buf. If this many hex
// digits are not available, return NULL. buf will not be NUL-terminated.
unsigned char *hextoascii(const char * restrict hex,unsigned char * restrict buf,
					int sep,size_t len){
	size_t off = 0;

	while(len){
		switch(hex[0]){
			case 'F':  case 'f':  buf[off] = 0xF0; break;
			case 'E':  case 'e':  buf[off] = 0xE0; break;
			case 'D':  case 'd':  buf[off] = 0xD0; break;
			case 'C':  case 'c':  buf[off] = 0xC0; break;
			case 'B':  case 'b':  buf[off] = 0xB0; break;
			case 'A':  case 'a':  buf[off] = 0xA0; break;
			case '9':  buf[off] = 0x90; break;
			case '8':  buf[off] = 0x80; break;
			case '7':  buf[off] = 0x70; break;
			case '6':  buf[off] = 0x60; break;
			case '5':  buf[off] = 0x50; break;
			case '4':  buf[off] = 0x40; break;
			case '3':  buf[off] = 0x30; break;
			case '2':  buf[off] = 0x20; break;
			case '1':  buf[off] = 0x10; break;
			case '0':  buf[off] = 0; break;
			default:
				bitch("Expected hex digit, got %s\n",hex);
				return NULL;
		}
		switch(hex[1]){
			case 'F':  case 'f':  buf[off] |= 0xF; break;
			case 'E':  case 'e':  buf[off] |= 0xE; break;
			case 'D':  case 'd':  buf[off] |= 0xD; break;
			case 'C':  case 'c':  buf[off] |= 0xC; break;
			case 'B':  case 'b':  buf[off] |= 0xB; break;
			case 'A':  case 'a':  buf[off] |= 0xA; break;
			case '9':  buf[off] |= 0x9; break;
			case '8':  buf[off] |= 0x8; break;
			case '7':  buf[off] |= 0x7; break;
			case '6':  buf[off] |= 0x6; break;
			case '5':  buf[off] |= 0x5; break;
			case '4':  buf[off] |= 0x4; break;
			case '3':  buf[off] |= 0x3; break;
			case '2':  buf[off] |= 0x2; break;
			case '1':  buf[off] |= 0x1; break;
			case '0':  break;
			default:
				bitch("Expected hex digpair, got %s\n",hex);
				return NULL;
		}
		--len;
		++buf;
		hex += 2;
		if(len){
			if(sep != EOF){
				if(*hex != sep){
					bitch("Expected %c, got %s\n",sep,hex);
					return NULL;
				}
				++hex;
			}
		}
	}
	return buf - off;
}

// Decode len characters from buf into a NUL-terminated ascii representation in
// hex, delimiting each hex pair with sep (which may be EOF). hex must provide
// 2*len+1 bytes, or 3*len+1 if sep != EOF. Result is always NUL-terminated.
void asciitohex(const void * restrict voidbuf,char * restrict hex,
				int sep,size_t len){
	const char *hexdig = "0123456789ABCDEF";
	const unsigned char *buf = voidbuf;
	int i = 0;

	while(len > 1){
		hex[i++] = hexdig[*buf >> 4];
		hex[i++] = hexdig[*buf & 0x0f];
		if(sep != EOF){
			hex[i++] = sep;
		}
		++buf;
		--len;
	}
	if(len){
		hex[i++] = hexdig[*buf >> 4];
		hex[i++] = hexdig[*buf & 0x0f];
		++buf;
		--len;
	}
	hex[i] = '\0';
}

// Like the above, but writes to a ustring; returns true on error
int us_asciitohex(const void * restrict voidbuf,ustring * restrict us_hex,
			int sep,size_t len){
	const unsigned char *buf = voidbuf;

	while(len > 1){
		if(sep != EOF){
		  if(printUString(us_hex,"%02hhX%c",*buf,sep) < 0) {
		    return -1;
		  }
		}else{
		  if(printUString(us_hex,"%02hhX",*buf) < 0) {
		    return -1;
		  }
		}
		++buf;
		--len;
	}
	if(len == 1){
	  if(printUString(us_hex,"%02hhX",*buf) < 0 ) {
	    return -1;
	  }
	  ++buf;
	  --len;
	}
	return 0;
}
