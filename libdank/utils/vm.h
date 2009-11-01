#ifndef LIBDANK_UTILS_VM
#define LIBDANK_UTILS_VM

#ifdef __cplusplus
extern "C" {
#endif

// Get the maximum page size as supported by the operating system in its
// current configuration (not necessarily the maximum page size for this
// hardware, which would go into arch/ as opposed to utils/). Special
// exertions might be necessary to use mappings with this page size (hugetlbfs
// on Linux, for instance).
size_t get_max_pagesize(void); 

#include <stdint.h>

// FIXME this is easily the least sane function i've ever declared --nlb
//
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
int setup_largepage_shmarea(const char *,size_t *,uintptr_t *);

#ifdef __cplusplus
}
#endif

#endif
