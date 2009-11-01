#ifndef LIBDANK_UTILS_RFC3330
#define LIBDANK_UTILS_RFC3330

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
	RFC3330_THISNETWORK,
	RFC3330_PRIVATE,
	RFC3330_PUBLICDATANET,
	RFC3330_LOOPBACK,
	RFC3330_LINKLOCAL,
	RFC3330_UNCLASSIFIED,
} rfc3330_type;

// Argument should be an IPv4 address in host byte-order
rfc3330_type categorize_ipv4address(uint32_t);

// 0.0.0.0/8 - Addresses in this block refer to source hosts on "this"
// network.  Address 0.0.0.0/32 may be used as a source address for this
// host on this network; other addresses within 0.0.0.0/8 may be used to
// refer to specified hosts on this network [RFC1700, page 4].
static inline int ipv4_is_rfc3330thisnetwork(uint32_t ip){
	return ip < 0x01000000;
}

// 10.0.0.0/8 - This block is set aside for use in private networks.
// 172.16.0.0/12 - This block is set aside for use in private networks.
// 192.168.0.0/16 - This block is set aside for use in private networks.
// Its intended use is documented in [RFC1918].  Addresses within this
// block should not appear on the public internet.
static inline int ipv4_is_rfc3330private(uint32_t ip){
	return ip < 0x0a000000 ? 0 : ip < 0x0b000000 ? 1 :
		ip < 0xac100000 ? 0 : ip < 0xac200000 ? 1 :
		ip < 0xc0a80000 ? 0 : ip < 0xc0a90000 ? 1 : 0;
}

// 14.0.0.0/8 - This block is set aside for assignments to the
// international system of Public Data Networks [RFC1700, page 181]. The
// registry of assignments within this block can be accessed from the
// "Public Data Network Numbers" link on the web page at
// http://www.iana.org/numbers.html.  Addresses within this block are
// assigned to users and should be treated as such.
static inline int ipv4_is_rfc3330publicdatanet(uint32_t ip){
	return (ip > 0x0dffffff && ip < 0x0f000000);
}

// 127.0.0.0/8 - This block is assigned for use as the internet host
// loopback address.  A datagram sent by a higher level protocol to an
// address anywhere within this block should loop back inside the host.
// This is ordinarily implemented using only 127.0.0.1/32 for loopback,
// but no addresses within this block should ever appear on any network
// anywhere [RFC1700, page 5].
static inline int ipv4_is_rfc3330loopback(uint32_t ip){
	return (ip > 0x7effffff && ip < 0x80000000);
}

// 169.254.0.0/16 - This is the "link local" block.  It is allocated for
// communication between hosts on a single link.  Hosts obtain these
// addresses by auto-configuration, such as when a DHCP server may not
// be found.
static inline int ipv4_is_rfc3330linklocal(uint32_t ip){
	return (ip > 0xa9fdffff && ip < 0xa9ff0000);
}

#ifdef __cplusplus
}
#endif

#endif
