#ifndef UTILS_RFC2396
#define UTILS_RFC2396

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct ustring;
struct sockaddr_in;

// FIXME update this for RFC 3986: http://labs.apache.org/webarch/uri/rfc/rfc3986.html#changes

// Verify that this is a legal URI by RFC 2396's specification, having the
// proper scheme (case-insensitive match). Advances the pointer past the URI.
// We're fairly tight regarding conformance; exceptions include:
//  "unwise" excluded characters are allowed in queries
int parse_uri(const char *,char **);

// The same, for RFC2817 CONNECT requests
int parse_connect_uri(char **);

typedef struct uri {
	// These are taken directly from RFC 2396's definitions. Any of them
	// may be NULL for a given URI:
	// The URI syntax does not require that the scheme-specific-part have
	// any general structure or set of semantics which is common among all
	// URI.  However, a subset of URI do share a common syntax for
	// representing hierarchical relationships within the namespace.  This
	// "generic URI" syntax consists of:
	//
	// <scheme>://<authority>/<path>?<query>#fragment, OR
	// <scheme>:/<path>?<query>#fragment, OR
	// <scheme>:opaque
	//
	// each of which may be absent from a particular URI. See RFC2396 5.2.
	char *scheme,*host,*userinfo,*path,*query,*fragment;
	unsigned port;
	char *opaque_part;
} uri;

uri *extract_uri(const char *,char **);
uri *extract_connect_uri(char **);
int uri_to_inetaddr(uri *,uint16_t,struct sockaddr_in *);
int set_uri_scheme(uri *,const char *);
int set_uri_host(uri *,const char *);
void free_uri(uri **);
int stringize_uri(struct ustring *,const uri *);

#ifdef __cplusplus
}
#endif

#endif
