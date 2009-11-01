#include <sys/mman.h>
#include <libdank/utils/mmap.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

// FIXME we ought be able to use MAP_SHARED|MAP_ANONYMOUS on linux since at
// least 2.4, but whenever we do, accessing the map following Mremap() gets a
// SIGBUS delivered...
static inline int
mmap_flags(int fd){
	if(fd < 0){
		return MAP_ANONYMOUS |
#ifdef MAP_NOSYNC
		MAP_SHARED | MAP_NOSYNC; // FreeBSD
#else
		MAP_PRIVATE; // Linux (see bugs 733, 864)
#endif
	}
	return MAP_SHARED
#ifdef MAP_NOSYNC
	| MAP_NOSYNC // FreeBSD
#endif
	;
}

// Initialize the windowed map, using the specified protection and size
int initialize_mmap_window(mmap_window *mw,int fd,int prot,size_t len){
	mw->mapbase = Mmap(NULL,len,prot,mmap_flags(fd),fd,0);
	if(mw->mapbase == MAP_FAILED){
		track_failloc();
		return -1;
	}
	track_allocation("mmap_window"); // FIXME take as argument?
	mw->maplen = len;
	mw->mapoff = 0;
	return 0;
}

// Release bytes from the front of the mmap
int shrink_mmap_window(mmap_window *mw,size_t shrink){
	if(shrink == 0 || shrink >= mw->maplen){
		bitch("Invalid argument (%zu)\n",shrink);
		return -1;
	}
	// FIXME we ought be using Mremap_fixed() for performance
	if(Munmap(mw->mapbase,shrink)){
		return -1;
	}
	mw->mapbase += shrink;
	mw->maplen -= shrink;
	mw->mapoff += shrink;
	return 0;
}

// Release bytes from the back of the mmap
int trim_mmap_window(mmap_window *mw,size_t shrink){
	if(shrink == 0 || shrink >= mw->maplen){
		bitch("Invalid argument (%zu)\n",shrink);
		return -1;
	}
	// FIXME we ought be using Mremap() for performance
	if(Munmap(mw->mapbase + (mw->maplen - shrink),shrink)){
		return -1;
	}
	mw->maplen -= shrink;
	return 0;
}

// Add bytes to the end of the map
int extend_mmap_window(mmap_window *mw,int fd,int prot,size_t extend){
	void *tmp;

	if(extend == 0){
		bitch("Invalid arguments (%zu)\n",extend);
		return -1;
	}
	tmp = Mremap(fd,mw->mapbase,mw->maplen,
		mw->maplen + extend,prot,mmap_flags(fd));
	if(tmp == MAP_FAILED){
		track_failloc();
		return -1;
	}
	mw->mapbase = tmp;
	mw->maplen += extend;
	return 0;
}

// Slide the map forward over the mapped object
int slide_mmap_window(mmap_window *mw,int fd,int prot,size_t delta){
	void *tmp;

	if(delta == 0 || mw->maplen <= 0){
		bitch("Invalid arguments (%zu, %zu)\n",delta,mw->maplen);
		return -1;
	}
	// FIXME ought be using remap_file_pages(2) on Linux for performance
	tmp = Mmap(mw->mapbase,mw->maplen,prot,mmap_flags(fd) | MAP_FIXED,
					fd,mw->mapoff + delta);
	if(tmp != mw->mapbase){
		bitch("Invalid MAP_FIXED result (%p, %p)\n",tmp,mw->mapbase);
		if(tmp != MAP_FAILED){
			Munmap(tmp,mw->maplen); // FIXME unsafe
		}
		track_failloc();
		return -1;
	}
	mw->mapoff += delta;
	return 0;
}

// Move the map back to the beginning of the underlying object (slide it left)
int reset_mmap_window(mmap_window *mw,int fd,int prot){
	void *tmp;

	if(mw->maplen <= 0){
		bitch("Invalid arguments (%zu)\n",mw->maplen);
		return -1;
	}
	if(fd >= 0){
		tmp = Mmap(mw->mapbase,mw->maplen,prot,mmap_flags(fd) | MAP_FIXED,fd,0);
		if(tmp != mw->mapbase){
			bitch("Invalid MAP_FIXED result (%p, %p)\n",tmp,mw->mapbase);
			if(tmp != MAP_FAILED){ // FIXME verify it doesn't overlap!
				Munmap(tmp,mw->maplen);
			}
			return -1;
		}
	}
	mw->mapoff = 0;
	return 0;
}

// Release the map in its entirety
int release_mmap_window(mmap_window *mw){
	int ret = 0;

	if(mw->maplen){
		ret |= Munmap(mw->mapbase,mw->maplen);
		track_deallocation();
		mw->mapbase = MAP_FAILED;
		mw->maplen = 0;
		mw->mapoff = 0;
	}
	return ret;
}
