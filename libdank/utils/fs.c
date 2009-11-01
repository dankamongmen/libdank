#include <libdank/utils/fs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/ersatz/compat.h>
#include <libdank/objects/logctx.h>
#ifdef LIB_COMPAT_LINUX
#include <sys/statfs.h>
#include <linux/magic.h>
#ifdef HUGETLBFS_MAGIC
#error "HUGETLBFS_MAGIC found! Remove the hardcoded entry in this file."
#else
// FIXME my patch to export this has been merged into Linux 2.6.31; we'll
// remove this code shortly (http://lkml.org/lkml/2009/6/29/523)
#define HUGETLBFS_MAGIC 0x958458f6
#endif
#define BASIC_TMPFS "tmpfs"
#define TMPFS_FLAGS (MS_NOATIME|MS_NODIRATIME)
#else
#ifdef LIB_COMPAT_FREEBSD
#include <sys/param.h>
#define BASIC_TMPFS "mdmfs"
#define TMPFS_FLAGS (MNT_NOATIME|MNT_ASYNC)
#else
#error "No support on this OS."
#endif
#endif
#include <sys/mount.h>

// Attempts to determine if the filesystem described by sfs is backed by system
// memory. This includes something backed by swap (which would actually be a
// disk), but disincludes virtual filesystems or romfs. Essentially, this
// answers the question, "can I mmap() transient files here?"
int fs_memorybacked(const struct statfs *sfs){
#ifdef LIB_COMPAT_FREEBSD
	const char *types[] = { BASIC_TMPFS, "mfs", 0 },*t;

	for(t = *types ; *t ; ++t){
		if(strcmp(sfs->f_fstypename,*types) == 0){
			return 1;
		}
	}
	nag("Filesystem type %s is not memory-backed\n",sfs->f_fstypename);
#else
#ifdef LIB_COMPAT_LINUX
	int magics[] = { HUGETLBFS_MAGIC, TMPFS_MAGIC, 0 },*m;

	for(m = magics ; *m ; ++m){
		if(sfs->f_type == *m){
			return 1;
		}
	}
	nag("Filesystem type %jd is not memory-backed\n",(intmax_t)sfs->f_type);
#else
#error "fs_memorybacked() not implemented on this OS"
#endif
#endif
	return 0;
}

// Attempts to determine if the filesystem described by sfs supports the
// largest pages offered by the hardware and OS. Essentially, this answers the
// question, "is this a good place to mmap() large transient files?"
int fs_largepagebacked(const struct statfs *sfs){
#ifdef LIB_COMPAT_FREEBSD
	// FIXME ensure that large page support is enabled via sysctl
	return fs_memorybacked(sfs);
#else
#ifdef LIB_COMPAT_LINUX
	return (unsigned long)sfs->f_type == HUGETLBFS_MAGIC;
#else
#error "fs_largepagebacked() not implemented on this OS"
#endif
#endif
}

// On FreeBSD, filesystems known to the kernel can be listed with lsvfs(1). On
// Linux, check /proc/filesystems.
int mount_tmpfs(const char *path){
	if(Mount(path,BASIC_TMPFS,TMPFS_FLAGS,NULL)){
		return -1;
	}
	return 0;
}
