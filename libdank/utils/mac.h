#ifndef UTILS_MAC
#define UTILS_MAC

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <net/ethernet.h>
#include <libdank/objects/objustring.h>

// maximum of guaranteed 12 hex digits, (ETH_ALEN - 1) separators, and 1 '\0'
#define ETH_ADDRSTRLEN (ETHER_ADDR_LEN * 2 + ETHER_ADDR_LEN)

int maccmp(const unsigned char *,const unsigned char *);

/* type MAC_STD: sep is the separating char, ie ':' gets you:
 *  "DE:AD:BE:EF:F0:0F" passing in EOF will give no separator,
 *  requiring only 13 bytes in buf. otherwise, you need 18.
 *  (12 %x,5 sep's,1 null terminator)
 * type MAC_HP: same, but only 1 sep char (for HP-style mac like
 *  (deadbe-eff00f) i.e. 14-byte buf
 * type MAC_CISCO: 2 sep chars (for cisco-style mac like
 *  (dead.beef.f00f) 15-byte buf */
typedef enum {
	MAC_STD,
	MAC_HP,
	MAC_CISCO
} mac_format_e;

char *mactoascii(const unsigned char * restrict,char * restrict,int,mac_format_e);

/* pass in an ETH_ALEN buffer for the mac.  sep==same rules as above. result
 * will *NOT* be null-termed (and is unsuitable for printing. return is -1 on
 * parse error, otherwise number of chars read */
int asciitomac(const char * restrict,unsigned char * restrict,int);

int uprintf_mac(ustring *,const unsigned char *,int);
int xmlize_mac(ustring *,const unsigned char *);

#ifdef __cplusplus
}
#endif

#endif
