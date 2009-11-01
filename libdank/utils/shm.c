#include <libdank/utils/shm.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

int create_shmfile(const char *name,int oflags,mode_t mode){
	int fd;

	if(name[0] != '/'){
		bitch("Not an absolute path: %s\n",name);
		return -1;
	}
#ifdef LIB_COMPAT_FREEBSD
	// FreeBSD sets certain file descriptor flags based on shm_open(3), so
	// we need to use it (Linux's just places things on the tmpfs)
	fd = Shm_open(name,oflags,mode);
#else
#ifdef LIB_COMPAT_LINUX
	// We don't want to use shm_open(3) on Linux, because it restricts us
	// to our IPC namespace's tmpfs (/dev/shm by default), meaning we
	// (a) collide with other processes and (b) can't use hugetlbfs. Thus,
	// we use open(2) directly, but enforce a VM-based filesystem.
	// FIXME enforce filesystem restrictions on Linux
	fd = OpenCreat(name,oflags,mode);
#else
	bitch("No support for %s, %x, %x on this OS\n",name,oflags,mode);
	fd = -1;
#endif
#endif
	return fd;
}

int open_shmfile(const char *name,int oflags){
	int fd;

	if(name[0] != '/'){
		bitch("Not an absolute path: %s\n",name);
		return -1;
	}
	// See notes from create_shmfile() on why we do this differently on
	// FreeBSD and Linux. We pass 0 as the mandatory mode argument to
	// shm_open(3) -- it won't be used.
#ifdef LIB_COMPAT_FREEBSD
	fd = Shm_open(name,oflags,0);
#else
#ifdef LIB_COMPAT_LINUX
	fd = Open(name,oflags);
#else
	bitch("No support for %s, %x on this OS\n",name,oflags);
	fd = -1;
#endif
#endif
	return fd;
}

int unlink_shmfile(const char *name){
	// See notes from create_shmfile() on why we do this differently on
	// FreeBSD and Linux.
#ifdef LIB_COMPAT_FREEBSD
	if(Shm_unlink(name)){
		return -1;
	}
#else
#ifdef LIB_COMPAT_LINUX
	if(Unlink(name)){
		return -1;
	}
#else
	bitch("No support for %s on this OS\n",name);
	return -1;
#endif
#endif
	return 0;
}
