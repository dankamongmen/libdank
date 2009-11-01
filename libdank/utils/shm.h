#ifndef LIBDANK_UTILS_SHM
#define LIBDANK_UTILS_SHM

#include <fcntl.h>

// The shm_open(3) interface of POSIX.1-2001 is limited and unpleasant to use.
// It handles the provided path differently on FreeBSD and Linux, maps all
// invocations to a single directory (usually /dev/shm) on Linux, and isn't
// integrated with Linux's huge page support (FreeBSD's superpages *are*
// closely integrated).
//
// This largely derives from SysV's "key"-based abstract shared memory IPC; all
// major modern implementations use either an explicit (Linux's tmpfs and
// hugetlbfs) or transparent (FreeBSD's superpages) filesystem-based approach.
// This API doesn't hide that fact; it's made plain that you're dealing with
// the filesystem and all that comes with it (file metadata, permissions, etc).
// All names passed must thus be absolute paths. For true shared memory 
// semantics on Linux, these paths must reside on a "tmpfs" or "hugetlbfs"
// filesystem. The create_shmfile() call enforces this, but neither
// open_shmfile() nor unlink_shmfile() do.
//
// Only mmap(2) can be used to read or write these file descriptors; Linux's
// hugetlbfs filesystem, for instance, does not support read(2) or write(2).
// Use of operations other than mmap(2) is non-portable. From FreeBSD 6.4's
// shm_open(3):
//
//   Only the O_RDONLY, O_RDWR, O_CREAT, O_EXCL, and O_TRUNC flags may be used
//   in portable programs.
//
//   The result of using open(2), read(2), or write(2) on a shared memory
//   object, or on the descriptor returned by shm_open(), is undefined. It is
//   also undefined whether the shared memory object itself, or its contents,
//   persist across reboots.
//
// Just like with regular files, circumstances might warrant use of filesystem
// mount options such as "async" and especially "noatime" for the shmfs.

// Create a new shmfile. O_CREAT must be passed; O_EXCL may be passed.
int create_shmfile(const char *,int,mode_t);

// Open a preexisting shmfile. Neither O_EXCL nor O_CREAT may be passed.
int open_shmfile(const char *,int);

// Unlink a shmfile.
int unlink_shmfile(const char *);

// FIXME let's also add helpers to:
//  - make a temporary (ie, randomly-named) shmfile in an (optional) directory
//     (use TMPDIR rules if no directory provided)
//  - check to see if a path is viable for shmfiles (always yes on freebsd,
//     except for write-only filesystems)
//  - set up a hugetlbfs?

#endif
