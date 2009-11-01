#ifndef LIBDANK_OBJECTS_SLALLOC
#define LIBDANK_OBJECTS_SLALLOC

// For basic usage and design concepts, see [Bonwick 1994], "The Slab
// Allocator: An Object-Caching Kernel Memory Allocator" and "Operating System
// Concepts" from Silberschatz, Galwin & Gagne. The design employed is similar
// to Lameter's SLUB allocator (although my development thereof, and surely
// countless others' as well, precedes its publication): no metadata within
// slabs proper, consideration of L1 and L2 parameters (detected at runtime),
// contention-avoidance techniques (per-CPU pools, etc) -- these are all
// obvious considerations. Research ideas include:
//
//  - optimizing for DRAM geometry viz CAS latencies (DRAM geometry is now
//     easily detected via SPD (for values of easily), making this a much more
//     reasonable problem than it'd have been years ago)
//  - generalized analysis of memory performance relative to parameterized
//     hierarchal models, especially regarding latency/throughput and possible
//     considerations of realtime issues (layout methodologies guaranteeing
//     latencies of operations based on memory models would surely be
//     publishable, if it hasn't already been done, which it surely has --
//     WHERE ARE THE PAPERS? THIS IS A DIFFICULT PROBLEM)
//  - metadata: how best to encode usemap for:
//     - space total
//     - space cachewise
//     - speed to find unused
//     - speed to count used

struct ustring;
struct slalloc;

// Base implementations; use DEFINE_SLALLOC_CACHE to define more typesafe
// variants for given object types.
struct slalloc *create_slalloc(size_t)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));
void destroy_slalloc(struct slalloc *);
void *slalloc_new(struct slalloc *)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));
int slalloc_free(struct slalloc *,void *);
int stringize_slalloc(struct ustring *,const struct slalloc *);
int slalloc_const_foreach(const struct slalloc *,const void *,int (*)(const void *,const void *));
int slalloc_foreach(struct slalloc *,void *,int (*)(void *,void *));
void slalloc_const_void_foreach(const struct slalloc *,const void *,void (*)(const void *,const void *));
void slalloc_void_foreach(struct slalloc *,void *,void (*)(void *,void *));

//objtype *slalloc_new_##objtype(struct slalloc_##objtype *); 
//
#define DEFINE_SLALLOC_CACHE(objtype) \
struct slalloc_##objtype; \
__attribute__ ((warn_unused_result)) __attribute__ ((malloc)) \
static inline struct slalloc_##objtype *create_slalloc_##objtype(void){ \
	return (struct slalloc_##objtype *)create_slalloc(sizeof(objtype)); \
} \
static inline void destroy_slalloc_##objtype(struct slalloc_##objtype *sl){ \
	destroy_slalloc((struct slalloc *)sl); \
} \
__attribute__ ((warn_unused_result)) __attribute__ ((malloc)) \
static inline objtype *slalloc_##objtype##_new(struct slalloc_##objtype *sl){ \
	return slalloc_new((struct slalloc *)sl); \
} \
static inline int stringize_slalloc_##objtype(struct ustring *u,const struct slalloc_##objtype *sl){ \
	return stringize_slalloc(u,(const struct slalloc *)sl); \
} \
static inline int slalloc_##objtype##_free(struct slalloc_##objtype *sl,objtype *o){ \
	return slalloc_free((struct slalloc *)sl,o); \
} \
static inline int slalloc_##objtype##_foreach(struct slalloc_##objtype *sl,void *state,int (*fxn)(void *,void *)){ \
	return slalloc_foreach((struct slalloc *)sl,state,fxn); \
} \
static inline int slalloc_##objtype##_const_foreach(const struct slalloc_##objtype *sl,const void *state,int (*fxn)(const void *,const void *)){ \
	return slalloc_const_foreach((const struct slalloc *)sl,state,fxn); \
} \
static inline void slalloc_##objtype##_void_foreach(struct slalloc_##objtype *sl,void *state,void (*fxn)(void *,void *)){ \
	slalloc_void_foreach((struct slalloc *)sl,state,fxn); \
} \
static inline void slalloc_##objtype##_const_void_foreach(const struct slalloc_##objtype *sl,const void *state,void (*fxn)(const void *,const void *)){ \
	slalloc_const_void_foreach((const struct slalloc *)sl,state,fxn); \
}

#endif
