#include <stddef.h>
#include <sys/mman.h>
#include <libdank/utils/magic.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/slalloc.h>

// Describes a bundle of pages (a block)
typedef struct pgdata {
	uint_fast32_t *usemap;
	unsigned unallocated;
	struct pgdata *next_nonfull;
} pgdata;

// We ought separate invariant initialization and per-use initialization into
// distinct logics via the API...
typedef struct slalloc {
	unsigned blockcount,pagesperblock;
	void **pgarray;
	pgdata *usemaps,*nonfull_usemaps;
	unsigned objsperpage;
	size_t objsize;
	size_t pgsize;
} slalloc;

// Usemap code -- accounting of page usage, stored in metadata (currently
// per-page usemaps). FIXME PoC implementation uses objsize <= pgsize, doesn't
// adjust for cache parameters, terrible all around

static inline unsigned
count_pages(const struct slalloc *sl){
	return sl->blockcount * sl->pagesperblock;
}

static inline ptrdiff_t
pgdata_idx(const slalloc *sl,const pgdata *pd){
	return pd - sl->usemaps;
}

static inline unsigned
objspermapword(const pgdata *pd){
	return sizeof(*pd->usemap) * CHAR_BIT;
}

// As it applies to the usemap, not to the actual mmap buffers!
#define OBJSPERWORD objspermapword(NULL)
#define OBJSPERBLOCK(sl) ((sl)->objsperpage * (sl)->pagesperblock)
#define OBJWORDSPERBLOCK(sl) (OBJSPERBLOCK(sl) / OBJSPERWORD + !!(OBJSPERBLOCK(sl) % OBJSPERWORD))
#define OBJSPERTHISWORD(sl,z) (((z) < OBJWORDSPERBLOCK(sl)) ? OBJSPERWORD : ((OBJSPERBLOCK(sl) % OBJSPERWORD)))

static int
init_usemap(slalloc *sl,pgdata *pd){
	if((pd->usemap = Malloc("usemap",sizeof(*pd->usemap) * OBJWORDSPERBLOCK(sl))) == NULL){
		return -1;
	}
	memset(pd->usemap,0,sizeof(*pd->usemap) * OBJWORDSPERBLOCK(sl));
	pd->unallocated = OBJSPERBLOCK(sl);
	pd->next_nonfull = sl->nonfull_usemaps;
	sl->nonfull_usemaps = pd;
	return 0;
}

static inline void
reset_usemap(pgdata *pd){
	if(pd){
		Free(pd->usemap);
	}
}

static void *
take_first_unused(slalloc *sl){
	void *ret = NULL;
	pgdata *map;

	if( (map = sl->nonfull_usemaps) ){
		unsigned z;

		for(z = 0 ; z < OBJWORDSPERBLOCK(sl) ; ++z){
			typeof(*map->usemap) rmost;
			unsigned idx;

			// Isolate the rightmost 0 bit (make it the sole 1)
			if((rmost = ~map->usemap[z] & (map->usemap[z] + 1ull)) == 0){
				continue;
			}
			// We needn't worry about OBJSPERTHISWORD effects. This
			// is easily proven by contradiction: assume that we
			// could use an invalid left bit on the final word of
			// the map due to objsperpage not being a multiple of
			// objsperword, returning I. In this case, there must
			// have been no free bit to the right, or else rmost
			// would not lead to I being returned. There must also
			// have been no other word with a free bit, or else it
			// would have been selected before this last word. In
			// that case, however, map->unallocated would have been
			// 0, due to being initialized to objsperpage. If map->
			// unallocated dropped to 0, however, this map would not
			// be on the nonfull list. We are on the nonfull list,
			// and thus the assumption is invalidated. QEMFD! -nlb
			map->usemap[z] |= rmost;
			if(--map->unallocated == 0){
				sl->nonfull_usemaps = map->next_nonfull;
			}
			// uintlog2(rmost) == 0-indexed position of high bit.
			// Dealloc is keyed off 1 <<= % OBJSPERWORD
			idx = z * OBJSPERWORD + uintlog2(rmost);
			// nag("idx %u z %u rmost %u\n",idx,z,(unsigned)rmost);
			return (char *)sl->pgarray[pgdata_idx(sl,map)] +
				idx / sl->objsperpage * sl->pgsize +
				(idx % sl->objsperpage) * sl->objsize;
		}
	}
	return ret;
}
// End usemap-dependent code

// Attempt to allocate pgs contiguous pages.
static void *
snatch_contiguous_pages(size_t pgsize,unsigned pgs){
	void *ret;

	// FIXME Rigorously predetecting overflow is kinda stupidly difficult.
	// It's arguable that a ceiling is wise in any case, but for now it's
	// necessary to ensure safety...we might (ought?) remove this.
#define MAX_CONTIGUOUS_PAGES 256
	if(pgs > MAX_CONTIGUOUS_PAGES){
		bitch("Allocating %u (>%d) contiguous pages isn't supported\n"
				,pgs,MAX_CONTIGUOUS_PAGES);
		errno = EINVAL; // see mmap(2)
		return NULL;
	}
#undef MAX_CONTIGUOUS_PAGES
	if((ret = Mmalloc("slalloc",pgsize * pgs)) == MAP_FAILED){
		ret = NULL;
	}else{
		memset(ret,0,pgsize * pgs);
	}
	return ret;
}

// If you know you want a number of pages, go ahead and try to grab a few to
// reduce syscall overhead and macrofragmentation. If we wanted, it'd be easy
// to scale back to 1 page on failure, but we don't support variable blocks.
static void *
fistful_of_pages(size_t pgsize,unsigned order){
	if(order > sizeof(order) * CHAR_BIT){
		bitch("Allocating 2^%u (>%zu) contiguous pages isn't supported\n"
				,order,sizeof(order) * CHAR_BIT);
		errno = EINVAL; // see mmap(2)
		return NULL;
	}
	return snatch_contiguous_pages(pgsize,1u << order);
}

static inline void
return_contiguous_pages(void *map,size_t pgsize,unsigned pgs){
	Mfree(map,pgs * pgsize);
}

slalloc *create_slalloc(size_t objsize){
	slalloc *ret = NULL;
	int pgsiz;

	if(objsize){
		if((pgsiz = Getpagesize()) > 0){
			if(objsize <= (unsigned)pgsiz){
				if( (ret = Malloc("slalloc",sizeof(*ret))) ){
					memset(ret,0,sizeof(*ret));
					ret->objsize = objsize;
					ret->objsperpage = (unsigned)pgsiz / ret->objsize;
					if((ret->pagesperblock = OBJSPERWORD / ret->objsperpage) == 0){
						ret->pagesperblock = 1;
					}else{
						// Isolate the rightmost 1 bit;
						// ppb must be a power of 2!
						ret->pagesperblock &= -ret->pagesperblock;
					}
					ret->pgsize = pgsiz;
				}
			}else{
				bitch("Won't slabcache objects larger than page (%zu > %d)\n",objsize,pgsiz);
			}
		}
	}else{
		bitch("Won't slabcache empty objects\n");
	}
	return ret;
}

void destroy_slalloc(slalloc *sl){
	if(sl){
		unsigned z;

		for(z = 0 ; z < sl->blockcount ; ++z){
			return_contiguous_pages(sl->pgarray[z],sl->pgsize,
						sl->pagesperblock);
			reset_usemap(&sl->usemaps[z]);
		}
		Free(sl->pgarray);
		Free(sl->usemaps);
		Free(sl);
	}
}

static int
add_slalloc_page(slalloc *sl){
	typeof(*sl->pgarray) *tmppgarray;
	typeof(*sl->usemaps) *tmpusemaps;
	size_t pgarrays,usemapss;
	void *newpages;

	pgarrays = sizeof(*sl->pgarray) * (sl->blockcount + 1);
	usemapss = sizeof(*sl->usemaps) * (sl->blockcount + 1);
	if((tmppgarray = Realloc("page array",sl->pgarray,pgarrays)) == NULL){
		return -1;
	}
	sl->pgarray = tmppgarray;
	if((tmpusemaps = Realloc("usemap",sl->usemaps,usemapss)) == NULL){
		return -1;
	}
	sl->usemaps = tmpusemaps;
	if(init_usemap(sl,&tmpusemaps[sl->blockcount])){
		return -1;
	}
	if((newpages = fistful_of_pages(sl->pgsize,uintlog2(sl->pagesperblock))) == NULL){
		reset_usemap(&tmpusemaps[sl->blockcount]);
		return -1;
	}
	sl->pgarray[sl->blockcount] = newpages;
	++sl->blockcount;
	return 0;
}

void *slalloc_new(slalloc *sl){
	void *ret = NULL;

	if(sl){
		if((ret = take_first_unused(sl)) == NULL){
			if(add_slalloc_page(sl) == 0){
				ret = take_first_unused(sl);
			}
		}
	}
	return ret;
}

// FIXME i'd really like this to be O(1) or at the very least O(lgN), PoC
static unsigned
find_block_idx(const slalloc *sl,const void *obj){
	unsigned z = 0;

	while((const char *)obj < (const char *)sl->pgarray[z] ||
		(const char *)obj >= (const char *)sl->pgarray[z] + sl->pgsize * sl->pagesperblock){
		++z;
	}
	return z;
}

// O(B) on the number of blocks B; see find_block_idx()
int slalloc_free(slalloc *sl,void *obj){
	typeof(*sl->usemaps->usemap) mask;
	unsigned idx,objidx,mapidx;
	size_t distance;

	idx = find_block_idx(sl,obj);
	distance = (const char *)obj - (const char *)sl->pgarray[idx];
	objidx = distance / sl->pgsize * sl->objsperpage +
	       		(distance % sl->pgsize) / sl->objsize;
	mapidx = objidx / OBJSPERWORD;
	mask = ~(1ull << (objidx % OBJSPERWORD));
	sl->usemaps[idx].usemap[mapidx] &= mask;
	memset(obj,0,sl->objsize); // FIXME we'd rather memset each on alloc
	// FIXME we likely want to put it near the back, not the front, as
	// it'll have only one element free (though more might be freed later,
	// suggesting we'd want to move it forward)...
	if(sl->usemaps[idx].unallocated++ == 0){
		sl->usemaps[idx].next_nonfull = sl->nonfull_usemaps;
		sl->nonfull_usemaps = &sl->usemaps[idx];
	}
	return 0;
}

int stringize_slalloc(struct ustring *u,const struct slalloc *sl){
	if(printUString(u,"<slalloc>") < 0){
		return -1;
	}
	if(printUString(u,"<objsize>%zu</objsize>",sl->objsize) < 0){
		return -1;
	}
	if(printUString(u,"<blocks>%u</blocks>",sl->blockcount) < 0){
		return -1;
	}
	if(printUString(u,"<pages>%u</pages>",count_pages(sl)) < 0){
		return -1;
	}
	if(printUString(u,"<objsperpage>%u</objsperpage>",sl->objsperpage) < 0){
		return -1;
	}
	if(printUString(u,"</slalloc>") < 0){
		return -1;
	}
	return 0;
}

#define FOREACH_CORE(sl,state,fxn,charcast) \
	unsigned p; \
 \
	for(p = 0 ; p < sl->blockcount ; ++p){ \
		unsigned z; \
 \
		for(z = 0 ; z < OBJWORDSPERBLOCK(sl) ; ++z){ \
			pgdata *map = &sl->usemaps[p]; \
			unsigned y; \
 \
			for(y = 0 ; y < OBJSPERTHISWORD(sl,z) ; ++y){ \
				if(map->usemap[z] & (1ull << y)){ \
					unsigned idx = z * OBJSPERWORD + y; \
 \
					if(fxn(state,(charcast)sl->pgarray[p] + \
						idx / (sl)->objsperpage * (sl)->pgsize + (idx % (sl)->objsperpage) * (sl)->objsize)){ \
						return -1; \
					} \
				} \
			} \
		} \
	} \
	return 0;

int slalloc_const_foreach(const slalloc *sl,const void *state,int (*fxn)(const void *,const void *)){
	FOREACH_CORE(sl,state,fxn,const char *);
}

int slalloc_foreach(slalloc *sl,void *state,int (*fxn)(void *,void *)){
	FOREACH_CORE(sl,state,fxn,char *);
}

#define FOREACH_VOID_CORE(sl,state,fxn,charcast) \
	unsigned p; \
 \
	for(p = 0 ; p < sl->blockcount ; ++p){ \
		unsigned z; \
 \
		for(z = 0 ; z < OBJWORDSPERBLOCK(sl) ; ++z){ \
			pgdata *map = &sl->usemaps[p]; \
			unsigned y; \
 \
			for(y = 0 ; y < OBJSPERTHISWORD(sl,z) ; ++y){ \
				if(map->usemap[z] & (1ull << y)){ \
					unsigned idx = z * OBJSPERWORD + y; \
 \
					fxn(state,(charcast)sl->pgarray[p] + \
						idx / (sl)->objsperpage * (sl)->pgsize + (idx % (sl)->objsperpage) * (sl)->objsize); \
				} \
			} \
		} \
	}

void slalloc_const_void_foreach(const slalloc *sl,const void *state,void (*fxn)(const void *,const void *)){
	FOREACH_VOID_CORE(sl,state,fxn,const char *);
}

void slalloc_void_foreach(slalloc *sl,void *state,void (*fxn)(void *,void *)){
	FOREACH_VOID_CORE(sl,state,fxn,char *);
}
