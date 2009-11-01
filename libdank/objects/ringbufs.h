#ifndef LIBDANK_OBJECTS_RINGBUFS
#define LIBDANK_OBJECTS_RINGBUFS

#ifdef __cplusplus
extern "C" {
#endif

#include <libdank/utils/mmap.h>

// Ring buffer using a single underlying mmap()able file descriptor (possibly a
// shared memory segment) -- presumably one we've created -- or an anonymous
// mapping. This builds atop the mmap_ring object, adding appropriate
// Ftruncate() calls, passing data back as a struct iov scatter-gather (of
// maximum size 2), and convenience (no need to pass the fd, save on
// initialization).
typedef struct scratchfile_ring {
	mmap_window mw;
	int fd;
} scratchfile_ring;

static inline int
initialize_scratchfile_ring(scratchfile_ring *rw,int fd,int prot,size_t len){
	if((rw->fd = fd) >= 0 && (prot & PROT_WRITE)){
		if(Ftruncate(fd,len)){
			return -1;
		}
	}
	if(initialize_mmap_window(&rw->mw,fd,prot,len)){
		return -1;
	}
	return 0;
}

static inline size_t
scratchfile_ring_totallen(const scratchfile_ring *rw){
	return mmap_window_totallen(&rw->mw);
}

static inline size_t
scratchfile_ring_maplen(const scratchfile_ring *rw){
	return mmap_window_maplen(&rw->mw);
}

// Return the character at the specified (virtual) offset. If the offset is
// invalid, returns -1 (we have to return an int anyway due to C calling
// conventions, so we're not sacrificing anything to include the sentinel).
static inline int
scratchfile_ring_charat(const scratchfile_ring *rw,size_t off){
	return mmap_window_charat(&rw->mw,off);
}

// Return a pointer to the character at the specified (virtual) offset. If the
// offset is invalid, returns NULL.
static inline char *
scratchfile_ring_ptrto(scratchfile_ring *rw,size_t off){
	return mmap_window_ptrto(&rw->mw,off);
}

static inline const char *
scratchfile_ring_const_ptrto(const scratchfile_ring *rw,size_t off){
	return mmap_window_const_ptrto(&rw->mw,off);
}

// Release bytes from the front of the mmap
static inline int
shrink_scratchfile_ring(scratchfile_ring *rw,size_t delta){
	return shrink_mmap_window(&rw->mw,delta);
}

// Release bytes from the back of the mmap and scratchfile
static inline int
trim_scratchfile_ring(scratchfile_ring *rw,size_t delta){
	if(trim_mmap_window(&rw->mw,delta)){
		return -1;
	}
	if(rw->fd >= 0){
		if(Ftruncate(rw->fd,scratchfile_ring_totallen(rw) - delta)){
			return -1; // but memory has been unmapped... FIXME
		}
	}
	return 0;
}

// Add bytes to the end of the map and scratchfile
static inline int
extend_scratchfile_ring(scratchfile_ring *rw,int prot,size_t delta){
	if((prot & PROT_WRITE) && rw->fd >= 0){
		if(Ftruncate(rw->fd,scratchfile_ring_totallen(rw) + delta)){
			return -1;
		}
	}
	if(extend_mmap_window(&rw->mw,rw->fd,prot,delta)){
		return -1; // but file has been extended... FIXME
	}
	return 0;
}

// Slide the map forward over the scratchfile
static inline int
slide_scratchfile_ring(scratchfile_ring *rw,int prot,size_t delta){
	if((prot & PROT_WRITE) && rw->fd >= 0){
		// is this the correct target length?
		if(Ftruncate(rw->fd,scratchfile_ring_totallen(rw) + delta)){
			return -1;
		}
	}
	return slide_mmap_window(&rw->mw,rw->fd,prot,delta);
}

// Move the map back to the beginning of the scratchfile (slide it backwards to
// a virtual offset of 0, and truncate)
static inline int
reset_scratchfile_ring(scratchfile_ring *rw,int prot){
	if(reset_mmap_window(&rw->mw,rw->fd,prot)){
		return -1;
	}
	if((prot & PROT_WRITE) && rw->fd >= 0){
		if(Ftruncate(rw->fd,scratchfile_ring_totallen(rw))){
			return -1; // but memory has been unmapped... FIXME
		}
	}
	return 0;
}

// Release the map in its entirety
static inline int
release_scratchfile_ring(scratchfile_ring *rw){
	rw->fd = -1;
	return release_mmap_window(&rw->mw);
}

#ifdef __cplusplus
}
#endif

#endif
