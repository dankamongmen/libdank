#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libdank/utils/parse.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/rfc2396.h>
#include <libdank/objects/lexers.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>

// RFC 2396, 2.2 "Reserved Characters"
static int
rfc2396_reserved_char(int c){
	return c == ';' || c == '/' || c == '?' || c == ':' || c == '@' ||
		c == '&' || c == '=' || c == '+' || c == '$' || c == ',';
}

// RFC 2396, 2.3 "Unreserved Characters"
static int
rfc2396_unreserved_char(int c){
	return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' ||
			c == '~' || c == '*' || c == '\'' || c == '(' ||
			c == ')';
}

// RFC 2396, 2.4.3 "Excluded US-ASCII Characters -- Unwise"
// FIXME As of RFC 2732, square brackets are "eserved within the authority
// component and are not allowed outside their use as delimiters for an IP
// literal within host" -- see RFC 3986, Appendix D
static int
rfc2396_unwise_char(int c){
	return c == '{' || c == '}' || c == '|' || c == '\\' || c == '^' ||
		c == '[' || c == ']' || c == '`';
}

// RFC 2396, 2.4 "Escape Sequences". We also accept an extension, %uXXXX where
// X are hexidecimal digits; this shows up in the field as an alternative
// method for encoding Unicode.
static int
rfc2396_escape_sequence(const char *c){
	if(c[0] == '%'){
		if(c[1] == 'u'){
			if(isxdigit(c[2]) && isxdigit(c[3]) &&
				isxdigit(c[4]) && isxdigit(c[5])){
				return 6;
			}
		}else if(isxdigit(c[1]) && isxdigit(c[2])){
			return 3;
		}
	}
	return 0;
}

// RFC 2396, 3.3 "Path Component"
static int
rfc2396_pchar(const char *c){
	int ret;

	if((ret = rfc2396_unreserved_char(*c)) || (ret = rfc2396_unwise_char(*c))
			|| (ret = rfc2396_escape_sequence(c))){
		return ret;
	}
	return *c == '/' || *c == ':' || *c == '@' || *c == '&' || *c == '='
		|| *c == '+' || *c == '$' || *c == ',' || *c == ';';
}

// RFC 2396, 3.4 "Query Component", Appendix A
// We also accept the "unwise" characters. This might be...unwise.
static int
rfc2396_qchar(const char *c){
	int ret;

	if( (ret = rfc2396_escape_sequence(c)) ){
		return ret;
	}
	return rfc2396_unreserved_char(*c) || rfc2396_reserved_char(*c)
		|| rfc2396_unwise_char(*c);
}

// Authorities are only present given certain preceeding characters, so we know
// we ought be an authority -- we needn't worry about being a path. There are,
// however, several types of authorities (lexically) we could be:
//  a) An RFC2732 IPv6 literal, enclosed in '[' ']' chars, and/or
//  b) Authentication material only followed by a hostname
//
// We only know if there's authentication material at the end of lexing it (due
// to the '@' character), and only know the meaning of ':' based off whether an
// '[' opened the hostname.
static int
parse_uri_authority(char ** const line,uri *u){
	const char *host;
	int l;

	if(*(host = *line) != '['){
		host = *line;
		while((l = rfc2396_unreserved_char(**line)) || (l = rfc2396_escape_sequence(*line))
				|| (l = (**line == ':'))){
			*line += l;
		}
		// FIXME need to handle "scheme-specific authentication syntaxes"
		if(**line == '@'){
			// Authority components beginning with a '@' are NOT allowed by
			// RFC2396. '@' could, however, begin a path, which is why it's
			// critical that authority parsing only happens with "//".
			if(*line == host){
				bitch("Empty URI user component\n");
				goto err;
			}
			if(u && (u->userinfo = Strndup(host,(size_t)(*line - host))) == NULL){
				goto err;
			}
			if(*(host = ++*line) != '['){ // RFC2732 + authentication
				while((l = rfc2396_unreserved_char(**line)) || (l = rfc2396_escape_sequence(*line))){
					*line += l;
				}
			}
		}
	}
	if(*line == host){
		if(**line != '['){
			bitch("Empty URI host component\n");
			goto err;
		}else{ // rfc2732 uri? let's see. iterate past the '['...
			char str[INET6_ADDRSTRLEN];
			struct in6_addr saddr;

			++*line;
			while(isxdigit(**line) || **line == '.' || **line == ':'){
				++*line;
			}
			if(**line != ']' || *line == host + 1){ // '[' is there
				bitch("Malformed IPv6 address literal\n");
				goto err;
			}
			if(*line - (host + 1) >= INET6_ADDRSTRLEN){
				bitch("Malformed IPv6 address literal\n");
				goto err;
			}
			memcpy(str,host + 1,*line - (host + 1));
			str[*line - (host + 1)] = '\0';
			if(Inet_pton(AF_INET6,str,&saddr) < 0){
				goto err;
			}
			++*line; // iterate past ']'; we want to include it
		}
	}
	if(u && ((u->host = Strndup(host,(size_t)(*line - host))) == NULL)){
		goto err;
	}
	if(**line == ':'){
		const char *port = ++*line;
		uint16_t p;

		if(lex_u16(&port,&p)){
			bitch("Invalid URI port component\n");
			goto err;
		}
		// Appendix G.3 says a bare ':' must be supported. FIXME we're
		// not preserving it across a stringize_uri() operation now...
		if(*line == port){
			nag("Empty URI port component\n");
		}
		if(u){
			u->port = p;
		}
		*line += port - *line; // avoiding cast
	}
	return 0;

err:
	if(u){
		Free(u->userinfo);
		u->userinfo = NULL;
		Free(u->host);
		u->host = NULL;
		Free(u->path);
		u->path = NULL;
		u->port = 0;
	}
	return -1;
}

static int
parse_uri_path(char **u,uri *ur){
	const char *path = *u;
	int l;

	while( (l = rfc2396_pchar(*u)) ){
		*u += l;
	}
	if(*u == path){
		nag("Empty URI path component\n");
	}else{
		if(ur && (ur->path = Strndup(path,(size_t)(*u - path))) == NULL){
			return -1;
		}
	}
	if(**u == '?'){
		const char *query = ++*u;

		while( (l = rfc2396_qchar(*u)) ){
			*u += l;
		}
		if(*u == query){
			nag("Empty URI query component\n");
		}
		// RFC 2396 3.4 allows an empty query following a '?'
		if(ur && (ur->query = Strndup(query,(size_t)(*u - query))) == NULL){
			goto err;
		}
	}
	if(**u == '#'){
		const char *fragment = ++*u;

		while( (l = rfc2396_pchar(*u)) ){
			*u += l;
		}
		if(*u == fragment){
			nag("Empty URI fragment component\n");
		}
		// RFC 2396 4.1 allows an empty fragment following a '#'
		if(ur && (ur->fragment = Strndup(fragment,(size_t)(*u - fragment))) == NULL){
			goto err;
		}
	}
	return 0;

err:
	if(ur){
		Free(ur->path);
		ur->path = NULL;
		Free(ur->fragment);
		ur->fragment = NULL;
		Free(ur->query);
		ur->query = NULL;
	}
	return -1;
}

static void
reset_uri(uri *u){
	if(u){
		Free(u->scheme);
		Free(u->host);
		Free(u->userinfo);
		Free(u->path);
		Free(u->query);
		Free(u->fragment);
		Free(u->opaque_part);
		memset(u,0,sizeof(*u));
	}
}

// FIXME this should probably reject more
static int
parse_uri_opaque(char ** const line,uri *ur){
	char *u = *line;

	while(*u && !isspace(*u)){
		++u;
	}
	if(ur && (ur->opaque_part = Strndup(*line,(size_t)(u - *line))) == NULL){
		return -1;
	}
	*line = u;
	return 0;
}

// A NULL scheme allows any scheme to be matched.
static int
extract_uri_internal(const char *scheme,char **line,uri *ur){
	char *u = *line,*schemestart;

	if(ur){
		memset(ur,0,sizeof(*ur));
	}
	parse_whitespaces(&u);
	schemestart = u;
	while(isalnum(*u) || *u == '-'){
		++u;
	}
	// RFC 2396 Section 3, 3.2, 5.2.7, Appendix A
	if(schemestart != u && *u == ':'){
		if(scheme && strncasecmp(schemestart,scheme,strlen(scheme))){
			bitch("Scheme didn't match \"%s\"\n",scheme);
			goto err;
		}
		if(ur && (ur->scheme = Strndup(schemestart,(size_t)(u - schemestart))) == NULL){
			goto err;
		}
		++u;
	}else{
		nag("No scheme\n");
		if(ur){
			ur->scheme = NULL;
		}
		u = schemestart;
	}
	if(*u == '/'){
		if(u[1] == '/'){
			u += 2;
			if(parse_uri_authority(&u,ur)){
				goto err;
			}
		}
		if(parse_uri_path(&u,ur)){
			goto err;
		}
	}else{
		if(parse_uri_opaque(&u,ur)){
			goto err;
		}
	}
	*line = u;
	return 0;

err:
	if(ur){
		reset_uri(ur);
	}
	return -1;
}

// RFC 2396 dictates this function's operation
int parse_uri(const char *scheme,char **line){
	return extract_uri_internal(scheme,line,NULL);
}

// This is used for RFC 2817 "CONNECT", which sends only an authority
int parse_connect_uri(char **line){
	return parse_uri_authority(line,NULL);
}

uri *extract_uri(const char *scheme,char **line){
	uri *ret;

	if( (ret = Malloc("RFC 2396 URI",sizeof(*ret))) ){
		if(extract_uri_internal(scheme,line,ret)){
			free_uri(&ret); // resets ret to NULL, so we can ret
		}
	}
	return ret;
}

uri *extract_connect_uri(char **line){
	uri *ret;

	if( (ret = Malloc("RFC 2817 URI",sizeof(*ret))) ){
		if(parse_uri_authority(line,ret)){
			free_uri(&ret); // resets ret to NULL, so we can ret
		}
	}
	return ret;
}

int uri_to_inetaddr(uri *u,uint16_t defport,struct sockaddr_in *sina){
	memset(sina,0,sizeof(*sina));
	if((sina->sin_port = u->port) == 0){
		sina->sin_port = defport;
	}
	sina->sin_port = htons(sina->sin_port);
	sina->sin_family = AF_INET;
	sina->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	// FIXME need to do lookup
	return 0;
}

int set_uri_scheme(uri *u,const char *scheme){
	char *tmp;

	if((tmp = Strdup(scheme)) == NULL){
		return -1;
	}
	Free(u->scheme);
	u->scheme = tmp;
	return 0;
}

int set_uri_host(uri *u,const char *host){
	char *tmp;

	if((tmp = Strdup(host)) == NULL){
		return -1;
	}
	Free(u->host);
	u->host = tmp;
	return 0;
}

void free_uri(uri **u){
	if(u && *u){
		reset_uri(*u);
		Free(*u);
		*u = NULL;
	}
}

static inline int
uri_has_authority_section(const uri *u){
	return u->host || u->userinfo || u->port;
}

// This is lain out very plainly in RFC 2396, 5.2.7
int stringize_uri(ustring *us,const uri *u){
	if(u->scheme){
		if(printUString(us,"%s:",u->scheme) < 0){
			return -1;
		}
	}
	if(u->opaque_part){
		if(printUString(us,"%s",u->opaque_part) < 0){
			return -1;
		}
		return 0;
	}
	if(uri_has_authority_section(u)){
		if(printUString(us,"//") < 0){
			return -1;
		}
		if(u->userinfo){
			if(printUString(us,"%s@",u->userinfo) < 0){
				return -1;
			}
		}
		if(u->host){
			if(printUString(us,"%s",u->host) < 0){
				return -1;
			}
		}
		if(u->port){
			if(printUString(us,":%hu",u->port) < 0){
				return -1;
			}
		}
	}
	if(u->path){
		if(printUString(us,"%s",u->path) < 0){
			return -1;
		}
	}
	if(u->query){
		if(printUString(us,"?%s",u->query) < 0){
			return -1;
		}
	}
	if(u->fragment){
		if(printUString(us,"#%s",u->fragment) < 0){
			return -1;
		}
	}
	return 0;
}
