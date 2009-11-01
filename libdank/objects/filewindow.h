#ifndef LIBDANK_OBJECTS_FILEWINDOW
#define LIBDANK_OBJECTS_FILEWINDOW

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <libdank/utils/mmap.h>
#include <libdank/utils/syswrap.h>

// Sliding window using a single underlying mmap()able file descriptor
// (possibly a shared memory segment) -- presumably one we've created -- or an
// anonymous mapping. This builds atop the mmap_window object, adding
// appropriate Ftruncate() calls and convenience (no need to pass the fd, save
// on initialization).
typedef struct scratchfile_window {
	mmap_window mw;
	int fd;
} scratchfile_window;

static inline int
initialize_scratchfile_window(scratchfile_window *sw,int fd,int prot,size_t len){
	if((sw->fd = fd) >= 0 && (prot & PROT_WRITE)){
		if(Ftruncate(fd,len)){
			return -1;
		}
	}
	if(initialize_mmap_window(&sw->mw,fd,prot,len)){
		return -1;
	}
	return 0;
}

static inline size_t
scratchfile_window_totallen(const scratchfile_window *sw){
	return mmap_window_totallen(&sw->mw);
}

static inline size_t
scratchfile_window_maplen(const scratchfile_window *sw){
	return mmap_window_maplen(&sw->mw);
}

// Return the character at the specified (virtual) offset. If the offset is
// invalid, returns -1 (we have to return an int anyway due to C calling
// conventions, so we're not sacrificing anything to include the sentinel).
static inline int
scratchfile_window_charat(const scratchfile_window *sw,size_t off){
	return mmap_window_charat(&sw->mw,off);
}

// Return a pointer to the character at the specified (virtual) offset. If the
// offset is invalid, returns NULL.
static inline char *
scratchfile_window_ptrto(scratchfile_window *sw,size_t off){
	return mmap_window_ptrto(&sw->mw,off);
}

static inline const char *
scratchfile_window_const_ptrto(const scratchfile_window *sw,size_t off){
	return mmap_window_const_ptrto(&sw->mw,off);
}

// Release bytes from the front of the mmap
static inline int
shrink_scratchfile_window(scratchfile_window *sw,size_t delta){
	return shrink_mmap_window(&sw->mw,delta);
}

// Release bytes from the back of the mmap and scratchfile
static inline int
trim_scratchfile_window(scratchfile_window *sw,int prot,size_t delta){
	if(trim_mmap_window(&sw->mw,delta)){
		return -1;
	}
	if((prot & PROT_WRITE) && sw->fd >= 0){
		if(Ftruncate(sw->fd,scratchfile_window_totallen(sw) - delta)){
			return -1; // but memory has been unmapped... FIXME
		}
	}
	return 0;
}

// Add bytes to the end of the map and scratchfile
static inline int
extend_scratchfile_window(scratchfile_window *sw,int prot,size_t delta){
	if((prot & PROT_WRITE) && sw->fd >= 0){
		if(Ftruncate(sw->fd,scratchfile_window_totallen(sw) + delta)){
			return -1;
		}
	}
	if(extend_mmap_window(&sw->mw,sw->fd,prot,delta)){
		return -1; // but file has been extended... FIXME
	}
	return 0;
}

// Slide the map forward over the scratchfile
static inline int
slide_scratchfile_window(scratchfile_window *sw,int prot,size_t delta){
	if((prot & PROT_WRITE) && sw->fd >= 0){
		// is this the correct target length?
		if(Ftruncate(sw->fd,scratchfile_window_totallen(sw) + delta)){
			return -1;
		}
	}
	return slide_mmap_window(&sw->mw,sw->fd,prot,delta);
}

// Move the map back to the beginning of the scratchfile (slide it backwards to
// a virtual offset of 0, and truncate)
static inline int
reset_scratchfile_window(scratchfile_window *sw,int prot){
	if(reset_mmap_window(&sw->mw,sw->fd,prot)){
		return -1;
	}
	if((prot & PROT_WRITE) && sw->fd >= 0){
		if(Ftruncate(sw->fd,scratchfile_window_totallen(sw))){
			return -1; // but memory has been unmapped... FIXME
		}
	}
	return 0;
}

// Release the map in its entirety
static inline int
release_scratchfile_window(scratchfile_window *sw){
	sw->fd = -1;
	return release_mmap_window(&sw->mw);
}

#ifdef __cplusplus
}
#endif

#endif
