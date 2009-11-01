#ifndef LIBDANK_UTILS_MMAP
#define LIBDANK_UTILS_MMAP

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

// Patterns for memory maps.
//
// ALL offsets and lengths must be multiples of the page size!

// Sliding window. We want to examine or build a map, starting from some
// offset (always 0 if we're constructing a file or anonymous map), without
// using space proportional to the length of the underlying datastream. We'll
// thus slide "to the right" (from beginning to end) over the data, having
// only a finite section mapped at any given time. This section might grow and
// shrink in size (logically, shrinking to 0 upon conclusion).
//
// "virtual offset" - offset relative to beginning of underlying file
// "relative offset" - offset relative to beginning of current map
//
// Growing a window might force us to move it, thus references to the map must
// be considered invalidated across calls to extend_mmap_window(). Shrinking a
// window might advance mapbase/mapoff, so relative offsets must be considered
// invalidated across calls to shrink_mmap_window(). Users thus ought retain
// only virtual offsets in state.
//
// Nothing about our design precludes disjoint mappings, or mapping multiple
// file descriptors, or even interleaving anonymous and file-backed map
// sections, except use of Linux's mremap(2) system call (which requires the
// entirety of the input to have been the output of a previous mmap(2) or
// mremap(2)). Pass -1 as the file descriptor to map anonymously.
typedef struct mmap_window {
	char *mapbase;	// the current memory map, having actual length of...
	size_t maplen;	// true (mmap()ed) length of mapbase, beginning at...
	off_t mapoff;	// offset of mapbase relative to the underlying object
} mmap_window;

// Initialize the windowed map, using the specified size and protection
int initialize_mmap_window(mmap_window *,int,int,size_t);

static inline size_t
mmap_window_totallen(const mmap_window *mw){
	return mw->mapoff + mw->maplen;
}

static inline size_t
mmap_window_maplen(const mmap_window *mw){
	return mw->maplen;
}

// Return the character at the specified (virtual) offset. If the offset is
// invalid, returns -1 (we have to return an int anyway due to C calling
// conventions, so we're not sacrificing anything to include the sentinel).
static inline int
mmap_window_charat(const mmap_window *mw,size_t off){
	if(off < (size_t)mw->mapoff || off - mw->mapoff > mw->maplen){
		return -1;
	}
	return mw->mapbase[off - mw->mapoff];
}

// Return a pointer to the character at the specified (virtual) offset. If the
// offset is invalid, returns NULL.
static inline char *
mmap_window_ptrto(mmap_window *mw,size_t off){
	if(off < (size_t)mw->mapoff || off - mw->mapoff > mw->maplen){
		return NULL;
	}
	return mw->mapbase + (off - mw->mapoff);
}

static inline const char *
mmap_window_const_ptrto(const mmap_window *mw,size_t off){
	if(off < (size_t)mw->mapoff || off - mw->mapoff > mw->maplen){
		return NULL;
	}
	return mw->mapbase + (off - mw->mapoff);
}

// Release bytes from the front of the mmap
int shrink_mmap_window(mmap_window *,size_t);

// Release bytes from the back of the mmap
int trim_mmap_window(mmap_window *,size_t);

// Add bytes to the end of the map
int extend_mmap_window(mmap_window *,int,int,size_t);

// Slide the map forward over the mapped object
int slide_mmap_window(mmap_window *,int,int,size_t);

// Move the map back to the beginning of the underlying object (slide it 
// backwards to a virtual offset of 0)
int reset_mmap_window(mmap_window *,int,int);

// Release the map in its entirety
int release_mmap_window(mmap_window *);

#ifdef __cplusplus
}
#endif

#endif
