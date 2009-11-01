#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

// Changes the semantics of FreeBSD's sendfile, which returns 0 on a complete
// write (we return the number of bytes written on success, and -1 otherwise)
// to match Linux's. Thus, callers looping until EAGAIN (ie, edge-triggered
// event handling) must check errno (EINTR will require tracking either the
// signal or still-pending tx). Also, we cease to support FreeBSD's headers or
// trailers, though we could do so with two writev() operations.
int sendfile_compat(int fd,int sd,off_t *off,size_t nbytes){
	off_t written = 0;
	int ret;

	// FreeBSD switches the sd's relative to Linux!
	ret = sendfile(sd,fd,*off,nbytes,NULL,&written,0);
	// written is updated even on failure for EBUSY, EINTR, and EAGAIN (we
	// can't draw EBUSY due to absence of SF_NODISKIO flag).
	*off += written;
	if(ret < 0){
		return -1;
	}
	return written;
}

// This is suitable really only for use with libdank's mremap_and_ftruncate(),
// due to assumptions it makes about the flags to pass to mmap(2). The only
// mremap(2) use case addressed is that of MREMAP_MAYMOVE. oldaddr must be a
// valid previous return from mmap(); NULL is not acceptable (ala Linux's
// mremap(2)), resulting in undefined behavior, despite realloc(3) semantics.
// Similarly, oldlen and newlen must be non-zero (and page-aligned).
void *mremap_compat(int fd,void *oldaddr,size_t oldlen,
				size_t newlen,int prot,int flags){
	void *ret;

	// From mmap(2) on freebsd 6.3: A successful FIXED mmap deletes any
	// previous mapping in the allocated address range. This means:
	// remapping over a current map will blow it away (unless FIXED isn't
	// provided, in which case it can't overlap an old mapping. See bug
	// 733 for extensive discussion of this issue for Linux and FreeBSD).
	if((ret = mmap((char *)oldaddr + oldlen,newlen - oldlen,prot,flags,fd,oldlen)) == MAP_FAILED){
		// We couldn't get the memory whatsoever (or we were a fresh
		// allocation that succeeded). Return the immediate result...
		return ret;
	} // ret != MAP_FAILED. Did we squash?
	if(ret != (char *)oldaddr + oldlen){
		// We got the memory, but not where we wanted it. Copy over the
		// old map, and then free it up...
		nag("Wanted %p, got %p\n",(char *)oldaddr + oldlen,ret);
		Munmap(ret,newlen - oldlen);
		if((ret = mmap(NULL,newlen,prot,flags,fd,0)) == MAP_FAILED){
			return ret;
		}
		memcpy(ret,oldaddr,oldlen);
		Munmap(oldaddr,oldlen); // Free the old mapping
		return ret;
	} // We successfully squashed. Return a pointer to the first buf.
	return oldaddr;
}

// Same as above, implementing MREMAP_MAYMOVE|MREMAP_FIXED
void *mremap_fixed_compat(int fd,void *oldaddr __attribute__ ((unused)),
			  size_t oldlen __attribute__ ((unused)),size_t newlen,
			  void *newaddr,int prot,int flags){
	void *ret;

	// From mmap(2) on freebsd 6.3: A successful FIXED mmap deletes any
	// previous mapping in the allocated address range. This means:
	// remapping over a current map will blow it away (unless FIXED isn't
	// provided, in which case it can't overlap an old mapping. See bug
	// 733 for extensive discussion of this issue for Linux and FreeBSD).
	if((ret = mmap(newaddr,newlen,prot,flags | MAP_FIXED,fd,0)) == MAP_FAILED){
		// Must've been a bad address+length. Propagate the failure.
		return ret;
	}
	// FIXME trim up the old map via 0, 1 or 2 munmap()s
	return ret;
}

static unsigned long openssl_id_idx;
static pthread_key_t openssl_id_key;
static pthread_once_t openssl_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t openssl_lock = PTHREAD_MUTEX_INITIALIZER;

static void
setup_openssl_idkey(void){
	if(pthread_key_create(&openssl_id_key,free)){
		// FIXME do what, exactly?
	}
}

// FIXME malloc attribute!
static unsigned long *
setup_new_sslid(void){
	unsigned long *ret;

	if( (ret = malloc(sizeof(*ret))) ){
		if(pthread_setspecific(openssl_id_key,ret) == 0){
			if(Pthread_mutex_lock(&openssl_lock) == 0){
				*ret = ++openssl_id_idx;
				return ret;
			}
			pthread_setspecific(openssl_id_key,NULL);
		}
		free(ret);
	}
	return NULL;
}

// OpenSSL requires a numeric identifier for threads. On FreeBSD (using
// the default or libthr implementations), pthread_self() is insufficient; it
// seems to return an aggregate... :/
unsigned long pthread_self_getnumeric(void){
	if(Pthread_once(&openssl_once,setup_openssl_idkey) == 0){
		unsigned long *key;

		if((key = pthread_getspecific(openssl_id_key)) || (key = setup_new_sslid())){
			return *key;
		}
	}
	// FIXME do what, exactly?
	return 0;
}
