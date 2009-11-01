#ifndef LIBDANK_OBJECTS_LRUPAT
#define LIBDANK_OBJECTS_LRUPAT

// We might want to use a PATRICIA trie to match strings, but not know those
// strings ahead of time. Furthermore, we might need to be robust against
// pedantic cases and attackers resulting in very, very long strings being
// matched. We would like to provide successful matching even for these
// strings, pursuant to QoS guarantees / resource allocation limits, but only
// so long as overall service cannnot be disrupted.

struct lrupat;
struct ustring;

struct lrupat *create_lrupat(void (*)(void *));
int add_lrupat(struct lrupat *,const char *,void *);
int add_lrupat_nocase(struct lrupat *,const char *,void *);
int lookup_lrupat(struct lrupat *,const char *,void **);
int lookup_lrupat_nocase(struct lrupat *,const char *,void **);
int lookup_lrupat_term(struct lrupat *,const char *,char,void **);
int lookup_lrupat_term_nocase(struct lrupat *,const char *,char,void **);
int lookup_lrupat_blob(struct lrupat *,const char *,size_t,void **);
void destroy_lrupat(struct lrupat *);
int stringize_lrupat(struct ustring *,const struct lrupat *);

#endif
