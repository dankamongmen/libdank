#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <libdank/utils/vm.h>
#include <libdank/utils/procfs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/lexers.h>
#ifdef LIB_COMPAT_FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

size_t get_max_pagesize(void){
	uintmax_t ret = 0;

#ifdef LIB_COMPAT_LINUX
#define MAXPAGETAG "Hugepagesize:"
#define PROC_MEMINFO "meminfo"
	if(procfile_tagged_uint(PROC_MEMINFO,MAXPAGETAG,&ret)){
		return -1;
	}
	// FIXME: Hugepagesize is expressed as "val kB" on my amd64 2.6.30. We
	// should almost certainly be using the parsed units to scale.
	ret *= 1024;
	if(ret == 0){
		int r;

		nag("Couldn't look up tag '%s' in procfs '%s'\n",
			MAXPAGETAG,PROC_MEMINFO);
		if((r = Getpagesize()) < 0 || (uintmax_t)r > SIZE_MAX){
			bitch("Invalid pagesize: %d\n",r);
			return 0;
		}
		ret = r;
	}else{
		nag("Largest supported pagesize: %ju\n",ret);
	}
#undef PROC_MEMINFO
#undef MAXPAGETAG
#else
#ifdef LIB_COMPAT_FREEBSD
	int r;

	// For FreeBSD 7.2 on, we ought get the superpage size FIXME
	if((r = Getpagesize()) < 0 || (uintmax_t)r > SIZE_MAX){
		bitch("Invalid pagesize: %d\n",r);
		return 0;
	}
	ret = r;
#else
	// FIXME Solaris has getpagesizes()...
	bitch("No support on this OS\n");
	ret = 0;
#endif
#endif
	return ret;
}

// Set up an area suitable for shared memory objects (as declared in
// libdank/utils/shm.h), implemented by as large a page size as is possible, at
// this path. The path must be a directory, should be empty, and must not be
// the mountpoint of any mounted filesystem, *unless* that filesystem is the
// type of filesystem setup_largepage_shmarea() would choose to set up anyway
// (it ought be empty in any case). At that point:
//
// - Determine the largest page size on this architecture.
// - Determine whether the OS can support this page size.
// - Determine whether the OS can support this page size at this path. On
//    Linux with multiple page sizes, we'll need a hugetlbfs mount, with pages
//    allocated. On Linux with a single page size, we'll need a hugetlbfs or
//    tmpfs mount, with space allocated. On FreeBSD, we'll want an mdmfs.
//    although shm_open(3)'s MAP_NOSYNC-like effects make it less important).
//    The OS must have the filesystem built in or available as a module, and
//    an acceptable filesystem must be mounted, or mountable.
//
// If the path is valid, and we end up with an appropriate filesystem, a page
// size being supported at this path will be stored into the value-result
// size_t parameter, the actual size of the filesystem (in bytes) is stored
// into the uintptr_t, and 0 is returned.
int setup_largepage_shmarea(const char *path,size_t *blksize,uintptr_t *total){
	struct statvfs vstat;
	struct statfs fsta;
	int fd;

	// FIXME will this work for a symlink to a directory? it ought!
	if((fd = Open(path,O_RDONLY)) < 0){
		return -1;
	}
	if(Fstatfs(fd,&fsta)){
		Close(fd);
		return -1;
	}
	if(Fstatvfs(fd,&vstat)){
		Close(fd);
		return -1;
	}
	if(Close(fd)){
		return -1;
	}
	nag("Read blocksize: %lu fragsize: %lu\n",vstat.f_bsize,vstat.f_frsize);
	// FIXME still plenty to do here!
	nag("using %s with %zu / %ju\n",path,*blksize,(uintmax_t)*total);
	return 0;
}
