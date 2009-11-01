#include <ctype.h>
#include <libdank/utils/string.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/lrupat.h>
#include <libdank/objects/slalloc.h>

#define SIGMA (1U << CHAR_BIT)

struct lpatnode {
	// FIXME replace these huge pointers (64 bits on amd64) with indices,
	// implementing bounded-population slalloc wrapper objects of 8, 16,
	// 32 and 64 bits....
	struct lpatnode *arr[SIGMA];
	void *obj;
	// If patstr is NULL, there is no key here; some member of arr[] is
	// valid, pointing further down the patricia trie. Otherwise, patstr is
	// a (possibly zero-length) ASCIIZ string, and the node is a tail.
	char *patstr;
};

typedef struct lpatnode lpatnode;

DEFINE_SLALLOC_CACHE(lpatnode);

typedef struct lrupat {
	lpatnode *nodes;
	struct slalloc_lpatnode *sl;
	void (*nwatchcb)(void *);
} lrupat;

lrupat *create_lrupat(void (*nwatchcb)(void *)){
	lrupat *ret = NULL;

	if( (ret = Malloc("lrupat",sizeof(*ret))) ){
		if((ret->sl = create_slalloc_lpatnode()) == NULL){
			Free(ret);
			return NULL;
		}
		ret->nwatchcb = nwatchcb;
		ret->nodes = NULL;
	}
	return ret;
}

static void
free_lrupat_chain(lrupat *lp,lpatnode **lps){
	lpatnode *l;

	if( (l = *lps) ){
		unsigned z;

		for(z = 0 ; z < sizeof((*lps)->arr) / sizeof(*(*lps)->arr) ; ++z){
			free_lrupat_chain(lp,&(*lps)->arr[z]);
		}
		slalloc_lpatnode_free(lp->sl,*lps);
	}
}


// Takes the patricia trie tail in *lps and flattens it out into a full chain
// (ie, de-patricizes it). To be used when a new entry being added collides
// with a preexisting trie tail.
static int
decay_lrupat_tail(lrupat *lp,lpatnode **lps){
	const char *suffix = (*lps)->patstr;
	lpatnode **chain,*chainhead,*last;

	if((*lps)->patstr == NULL || *(*lps)->patstr == '\0'){
		bitch("Invalid tail %p\n",(*lps)->patstr);
		return -1;
	}
	chain = &chainhead;
	// We know suffix != '\0' from the check above. Thus, last will be
	// assigned at some point through here. We initialize it to NULL to
	// avoid a compiler warning for now FIXME.
	if((*chain = slalloc_lpatnode_new(lp->sl)) == NULL){
		return -1;
	}
	last = *chain;
	do{
		chain = &(*chain)->arr[*(const unsigned char *)suffix];
		if((*chain = slalloc_lpatnode_new(lp->sl)) == NULL){
			free_lrupat_chain(lp,&chainhead);
			return -1;
		}
		// nag("expanded [%s] into %p\n",suffix,*chain);
		last = *chain;
	}while(*++suffix);
	if((last->patstr = Strdup(suffix)) == NULL){
		free_lrupat_chain(lp,&chainhead);
		return -1;
	}
	// nag("set patstr in %p: [%s]\n",last,last->patstr);
	last->obj = (*lps)->obj;
	(*lps)->obj = NULL;
	Free((*lps)->patstr);
	(*lps)->patstr = NULL;
	*lps = chainhead;
	return 0;
}

// Takes the patricia trie tail in *lps and flattens it out into a full chain
// (ie, de-patricizes it). To be used when a new entry being added collides
// with a preexisting trie tail.
static int
decay_lrupat_tail_nocase(lrupat *lp,lpatnode **lps){
	const char *suffix = (*lps)->patstr;
	lpatnode **chain,*chainhead,*last;
	char *cur;

	if((*lps)->patstr == NULL || *(*lps)->patstr == '\0'){
		bitch("Invalid tail %p\n",(*lps)->patstr);
		return -1;
	}
	chain = &chainhead;
	// We know suffix != '\0' from the check above. Thus, last will be
	// assigned at some point through here. We initialize it to NULL to
	// avoid a compiler warning for now FIXME.
	if((*chain = slalloc_lpatnode_new(lp->sl)) == NULL){
		return -1;
	}
	last = *chain;
	do{
		chain = &(*chain)->arr[tolower(*(const unsigned char *)suffix)];
		if((*chain = slalloc_lpatnode_new(lp->sl)) == NULL){
			free_lrupat_chain(lp,&chainhead);
			return -1;
		}
		// nag("expanded [%s] into %p\n",suffix,*chain);
		last = *chain;
	}while(*++suffix);
	if((last->patstr = Strdup(suffix)) == NULL){
		free_lrupat_chain(lp,&chainhead);
		return -1;
	}
	for(cur = last->patstr ; *cur ; ++cur){
		*cur = tolower(*cur);
	}
	// nag("set patstr in %p: [%s]\n",last,last->patstr);
	last->obj = (*lps)->obj;
	(*lps)->obj = NULL;
	Free((*lps)->patstr);
	(*lps)->patstr = NULL;
	*lps = chainhead;
	return 0;
}

static lpatnode *
lpatchain_new(lrupat *lp,const char *key,void *obj){
	lpatnode *lpn;

	if((lpn = slalloc_lpatnode_new(lp->sl)) == NULL){
		return NULL;
	}
	if((lpn->patstr = Strdup(key)) == NULL){
		slalloc_lpatnode_free(lp->sl,lpn);
		return NULL;
	}
	// nag("set patstr: [%s]\n",lpn->patstr);
	lpn->obj = obj;
	return lpn;
}

static lpatnode *
lpatchain_nocase_new(lrupat *lp,const char *key,void *obj){
	lpatnode *lpn;
	char *cur;

	if((lpn = slalloc_lpatnode_new(lp->sl)) == NULL){
		return NULL;
	}
	if((lpn->patstr = Strdup(key)) == NULL){
		slalloc_lpatnode_free(lp->sl,lpn);
		return NULL;
	}
	for(cur = lpn->patstr ; *cur ; ++cur){
		*cur = tolower(*cur);
	}
	// nag("set patstr: [%s]\n",lpn->patstr);
	lpn->obj = obj;
	return lpn;
}

static int
add_lrupat_nocase_helper(lrupat *lp,lpatnode **lps,const char *key,void *obj){
	// nag("%p %p [%s] %p\n",lp,lps,key,obj);
	if(*lps == NULL){
		if((*lps = lpatchain_nocase_new(lp,key,obj)) == NULL){
			return -1;
		}
		return 0;
	}
	if(*key){
		if((*lps)->patstr){
			if(*(*lps)->patstr){
				// nag("expanding [%s] existing tail [%s]\n",key,(*lps)->patstr);
				if(decay_lrupat_tail_nocase(lp,lps)){
					return -1;
				}
			}
		}
		if(add_lrupat_nocase_helper(lp,&(*lps)->arr[tolower(*(const unsigned char *)key)],key + 1,obj)){
			return -1;
		}
	}else{
		typeof (*(*lps)->patstr) *tmp,*cur;

		if((tmp = Strdup(key)) == NULL){
			return -1;
		}
		for(cur = tmp ; *cur ; ++cur){
			*cur = tolower(*cur);
		}
		if((*lps)->patstr){
			if(*(*lps)->patstr){
				// nag("expanding [%s] existing tail [%s]\n",key,(*lps)->patstr);
				if(decay_lrupat_tail_nocase(lp,lps)){
					return -1;
				}
			}else{
				// nag("There was already a key here (%p)\n",(*lps)->obj);
				if(lp->nwatchcb){
					lp->nwatchcb((*lps)->obj);
				}
				Free((*lps)->patstr);
			}
		}
		(*lps)->patstr = tmp;
		// nag("set patstr: [%s]\n",(*lps)->patstr);
		(*lps)->obj = obj;
	}
	return 0;
}

static int
add_lrupat_helper(lrupat *lp,lpatnode **lps,const char *key,void *obj){
	// nag("%p %p [%s] %p\n",lp,lps,key,obj);
	if(*lps == NULL){
		if((*lps = lpatchain_new(lp,key,obj)) == NULL){
			return -1;
		}
		return 0;
	}
	if(*key){
		if((*lps)->patstr){
			if(*(*lps)->patstr){
				// nag("expanding [%s] existing tail [%s]\n",key,(*lps)->patstr);
				if(decay_lrupat_tail(lp,lps)){
					return -1;
				}
			}
		}
		if(add_lrupat_helper(lp,&(*lps)->arr[*(const unsigned char *)key],key + 1,obj)){
			return -1;
		}
	}else{
		typeof (*(*lps)->patstr) *tmp;

		if((tmp = Strdup(key)) == NULL){
			return -1;
		}
		if((*lps)->patstr){
			if(*(*lps)->patstr){
				// nag("expanding [%s] existing tail [%s]\n",key,(*lps)->patstr);
				if(decay_lrupat_tail(lp,lps)){
					return -1;
				}
			}else{
				// nag("There was already a key here (%p)\n",(*lps)->obj);
				if(lp->nwatchcb){
					lp->nwatchcb((*lps)->obj);
				}
				Free((*lps)->patstr);
			}
		}
		(*lps)->patstr = tmp;
		// nag("set patstr: [%s]\n",(*lps)->patstr);
		(*lps)->obj = obj;
	}
	return 0;
}

int add_lrupat(lrupat *lp,const char *key,void *obj){
	return add_lrupat_helper(lp,&lp->nodes,key,obj);
}

int add_lrupat_nocase(lrupat *lp,const char *key,void *obj){
	return add_lrupat_nocase_helper(lp,&lp->nodes,key,obj);
}

static int
lookup_lrupat_helper(lpatnode *lps,const char *key,char term,void **obj){
	// nag("%p [%s]\n",lps,key);
	if(lps == NULL){
		return 0;
	}
	if(lps->patstr){
		// FIXME this all seems rather terribly suboptimal
		size_t patlen = strlen(lps->patstr);

		// nag("found patstr [%s] we have [%s]\n",lps->patstr,key);
		if(strncmp(key,lps->patstr,patlen) == 0 && key[patlen] == term){
			*obj = lps->obj;
			return 1;
		}
		// return 0;
	}
	if(*key && (*key != term)){
		// nag("moving based on %c\n",*key);
		return lookup_lrupat_helper(lps->arr[*(const unsigned char *)key],key + 1,term,obj);
	}
	return 0;
}

int lookup_lrupat(lrupat *lp,const char *key,void **obj){
	return lookup_lrupat_helper(lp->nodes,key,'\0',obj);
}

int lookup_lrupat_term(lrupat *lp,const char *key,char term,void **obj){
	return lookup_lrupat_helper(lp->nodes,key,term,obj);
}

static int
lookup_lrupat_nocase_helper(lpatnode *lps,const char *key,char term,void **obj){
	// nag("%p [%s]\n",lps,key);
	if(lps == NULL){
		return 0;
	}
	if(lps->patstr){
		// FIXME this all seems rather terribly suboptimal
		size_t patlen = strlen(lps->patstr);

		// nag("found patstr [%s] we have [%s]\n",lps->patstr,key);
		if(strncasecmp(key,lps->patstr,patlen) == 0 && key[patlen] == term){
			*obj = lps->obj;
			return 1;
		}
		// return 0;
	}
	if(*key && (*key != term)){
		// nag("moving based on %c:%s\n",tolower(*(const unsigned char *)key),key + 1);
		return lookup_lrupat_nocase_helper(lps->arr[tolower(*(const unsigned char *)key)],key + 1,term,obj);
	}
	return 0;
}

int lookup_lrupat_nocase(lrupat *lp,const char *key,void **obj){
	return lookup_lrupat_nocase_helper(lp->nodes,key,'\0',obj);
}

int lookup_lrupat_term_nocase(lrupat *lp,const char *key,char term,void **obj){
	return lookup_lrupat_nocase_helper(lp->nodes,key,term,obj);
}

static int
lookup_lrupat_blob_helper(lpatnode *lps,const char *key,size_t len,void **obj){
	while(lps){
		if(lps->patstr){
			if(strcmp(lps->patstr,key)){
				return 0;
			}
			*obj = lps->obj;
			return 1;
		}
		if(len == 0){
			return 0;
		}
		if((lps = lps->arr[*(const unsigned char *)key++]) == NULL){
			return 0;
		}
		--len;
	}
	return 0;
}

int lookup_lrupat_blob(lrupat *lp,const char *key,size_t len,void **obj){
	return lookup_lrupat_blob_helper(lp->nodes,key,len,obj);
}

static void
free_lpatnode(void *s,void *v){
	lpatnode *lps = v;
	lrupat *lp = s;

	if(lp->nwatchcb){
		lp->nwatchcb(lps->obj);
	}
	Free(lps->patstr);
	lps->patstr = NULL;
}

void destroy_lrupat(lrupat *lp){
	if(lp){
		slalloc_lpatnode_void_foreach(lp->sl,lp,free_lpatnode);
		destroy_slalloc_lpatnode(lp->sl);
		Free(lp);
	}
}

int stringize_lrupat(ustring *u,const lrupat *lp){
	if(printUString(u,"<lrupat>") < 0){
		return -1;
	}
	if(stringize_slalloc_lpatnode(u,lp->sl)){
		return -1;
	}
	if(printUString(u,"</lrupat>") < 0){
		return -1;
	}
	return 0;
}
