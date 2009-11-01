#ifndef UTILS_MEMLIMIT
#define UTILS_MEMLIMIT

#ifdef __cplusplus
extern "C" {
#endif

// with the linux kernel's overcommit/OOM policy, we can't be
// assured of robust dynamic allocation.  rather than returning
// NULL, we may get a valid pointer which results in a
// SIG{SEGV|KILL} when the memory is used.  furthermore, we need
// to reserve memory for various system components.
//
// apps should allow a maximum memory usage to be supplied via
// runtime configuration, register that, and then use the malloc
// wrapper herein to ensure NULL returns and limiting.
//
// All but Realloc() initialize returned memory to 0's, both to
// guard against most initialization errors and to normalize the
// effects of aforementioned overcommit. Sizes of 0 will result
// in failure, including in Realloc().

#include <stdint.h>
#include <libdank/objects/objustring.h>

int limit_memory(size_t);

int64_t outstanding_allocs(void);
int stringize_memory_usage(ustring *);

// We need a Calloc() // FIXME
void *Malloc(const char *,size_t)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

void *Realloc(const char *,void *,size_t)
	__attribute__ ((warn_unused_result));

void *Palloc(const char *,size_t,unsigned *,unsigned)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

void Deepfree(const char *,void *); // only to be used for Free(), fpointers!
#define Free(ptr) Deepfree(__func__,ptr)

void *Mmalloc(const char *,size_t)
	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));

int Mfree(void *,size_t);

// Check if it's acceptable to perform an allocation from an external allocator
int track_allocation(const char *);
void track_deallocation(void);
void track_failloc(void);

void failloc_on_n(int64_t); // only allow up through n allocations

struct logctx;

void deeperfree(struct logctx *); // only to be called by free_logctx()!

// we can't portably mremap(2) -- it's only available on Linux. on freebsd,
// however, we can mmap at a location hint, and if it succeeds, it will destroy
// all preexisting mappings(!). we need the prot, flags and fd info, though.
// this wouldn't be available to us in userspace save through /proc/*/map, but
// if we restrict ourselves to filesemantics-backed objects (ie, everything but
// MAP_ANON, including shared mem or maps of /dev/zero (see APIUE2 chapter 15),
// we need to do a ftruncate (lseek+write, really, to be truly portable). this
// means:
//  - we have the fd
//  - we know we have PROT_WRITE, which pretty much implies PROT_READ
//  - we're broken for PROT_EXEC -- maybe look back up through fd? FIXME
//  - we don't have the flags but can likely determine them hackishly FIXME
// Furthermore, since FreeBSD won't have the MREMAP flags defined, we use that
// to determine whether mremap(2) is available at all (dangerous -- they
// require GNU to export), and spare the caller the requirement of providing
// them (assuming MREMAP_MAYMOVE always).
void *mremap_and_truncate(int,void *,size_t,size_t,int,int);

// Munmap an area allocated via mremap_and_truncate(). Necessary for proper
// libdank allocation tracking (see bug 735).
int mremap_munmap(void *,size_t);

#ifdef __cplusplus
}
#endif

#endif
