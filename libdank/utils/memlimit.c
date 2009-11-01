#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>

#define KIBIBYTE ((size_t)1024)
#define MIBIBYTE (KIBIBYTE * KIBIBYTE)
#define GIBIBYTE (KIBIBYTE * MIBIBYTE)
static const size_t DEFAULT_APP_MEMLIMIT = 256 * MIBIBYTE;

static size_t static_usage;	// XXX doesn't work yet
static intmax_t allocs_used;	// allocs served and not yet freed (current)
static uintmax_t allocs_reqd;	// alloc requests (lifetime)
static uintmax_t allocs_fail;	// alloc failures (lifetime)

static intmax_t poisoned_idx = -1;	// (testing) fail in idx allocs
// 0 means uninitialized, don't allow allocations (providing 0 to
// limit_memory() will force a cap based on free memory advertised)
static uintmax_t memory_usage_limit;
static pthread_mutex_t memlock = PTHREAD_MUTEX_INITIALIZER;

#ifdef MEMSTAT_METHOD_SYSINFO
#include <malloc.h>
static inline uintmax_t
mem_used(void){
	struct mallinfo mi;

	// mallinfo() is ungodly slow (thanks a whole bunch for that return of
	// an aggregate, glibc!), but all attempts to cache its results and
	// self-account for Malloc()s (Free() and Realloc() obviously require
	// actually checking the memory layer) have failed.
	mi = mallinfo();
	// mi.hblkhd is documented to return the amount of memory used by
	// explicitly mmap()ed regions, but seems to have breakage...?
	return mi.arena + mi.hblkhd;
}
#else
#ifdef MEMSTAT_METHOD_SYSCTL
/* These stats are per-process, and thus, under LinuxThreads, per-thread. We
 * somehow need to aggregate them....blech FIXME Hack for LinuxThreads
static struct rusage ru_cache;
static long ru_maxrss_max;

static inline uintmax_t
mem_used(void){
	Getrusage(RUSAGE_SELF,&ru_cache);
	if(ru_cache.ru_maxrss > ru_maxrss_max){
		ru_maxrss_max = ru_cache.ru_maxrss;
	}
	return ru_maxrss_max * KIBIBYTE;
} */
static inline uintmax_t
mem_used(void){
	struct rusage ru;

	Getrusage(RUSAGE_SELF,&ru);
	return ru.ru_maxrss * KIBIBYTE;
}
#else
#error "No method for accessing memory statistics is defined!"
#endif
#endif

int stringize_memory_usage(ustring *u){
	uintmax_t reqd,fail;
	intmax_t used;

	pthread_mutex_lock(&memlock);
	used = allocs_used;
	reqd = allocs_reqd;
	fail = allocs_fail;
	pthread_mutex_unlock(&memlock);
	if(printUString(u,"<memstats><limit>%ju</limit>"
			"<usedtotal>%ju</usedtotal>"
			"<req>%ju</req>"
			"<failedreq>%ju</failedreq>"
			"<outstanding>%jd</outstanding>"
			"</memstats>"
			,memory_usage_limit,mem_used(),
			reqd,fail,used) < 0){
		return -1;
	}
	return 0;
}

intmax_t outstanding_allocs(void){
	intmax_t used;

	pthread_mutex_lock(&memlock);
	used = allocs_used;
	pthread_mutex_unlock(&memlock);
	return used;
}

#ifdef MEMSTAT_METHOD_SYSINFO
#include <sys/sysinfo.h>
#else
#ifdef MEMSTAT_METHOD_SYSCTL
#include <sys/sysctl.h>
#endif
#endif

static uintmax_t
determine_static_usage(void){
	struct rusage ru;
	uintmax_t ret;

	if(Getrusage(RUSAGE_SELF,&ru)){
		return DEFAULT_APP_MEMLIMIT; // take a guess for safety
	}
	ret = ru.ru_maxrss * KIBIBYTE;
	nag("Max resident set: %jdb\n",ret);
	return ret;
}

static uintmax_t
determine_sysmem(void){
	struct rlimit rlim;

	if(Getrlimit(RLIMIT_AS,&rlim) == 0){
		if(rlim.rlim_cur <= 0 || rlim.rlim_cur == RLIM_INFINITY){
			nag("RLIMIT_AS meaningless: %lld\n",(long long)rlim.rlim_cur);
		}else{
			nag("Rlimit: %lld free\n",(long long)rlim.rlim_cur);
			return rlim.rlim_cur;
		}
	}
	{
#ifdef MEMSTAT_METHOD_SYSINFO
		struct sysinfo si;

		if(Sysinfo(&si) == 0){
			uintmax_t totalram,freeram;

			totalram = si.mem_unit * si.totalram;
			freeram = si.mem_unit * si.freeram;
			nag("Sysinfo: %ju of %ju free\n",freeram,totalram);
			return totalram;
		}
#else
#ifdef MEMSTAT_METHOD_SYSCTL
		long totalram,freeram;
		size_t trsiz,frsiz;

		trsiz = sizeof(totalram);
		frsiz = sizeof(freeram);
		if(Sysctlbyname("hw.physmem",&totalram,&trsiz,NULL,0) == 0 &&
			Sysctlbyname("hw.usermem",&freeram,&frsiz,NULL,0) == 0){

			nag("Sysctl: %ld of %ld available to userspace\n",freeram,totalram);
			return totalram;
		}
#endif
#endif
	}
	return 0;
}
	
// Provide the maximum memory size in bytes. It will be checked against free
// memory (*not* claimed memory; we do not know other reservations), and capped
// at 50% thereof. If the limit is far too low for a sane app, this will also
// result in a -1 return (mainly to catch mistakes involving units). A limit
// of 0 implies capping only wrt known free memory.
int limit_memory(size_t s){
	int ret = 0;

	if(memory_usage_limit){
		bitch("Not resetting memlimit of %jub\n",memory_usage_limit);
		return -1;
	}
	if(s){
		nag("Provided a memory limit of %zub\n",s);
	}else{
		s = DEFAULT_APP_MEMLIMIT;
	}
	memory_usage_limit = determine_sysmem();
	static_usage = determine_static_usage();
	nag("Preexisting allocation total: %zu\n",static_usage);
	s += static_usage;
	if(s > memory_usage_limit){
		bitch("Capping %zub to syslimit %jub\n",s,memory_usage_limit);
		ret = -1;
	}else if(s){
		struct rlimit rl = { .rlim_cur = s, .rlim_max = s, };

		ret |= Setrlimit(RLIMIT_AS,&rl);
		memory_usage_limit = s;
	}
	nag("Setting memlimit: %ju MiB\n",memory_usage_limit / MIBIBYTE);
	return ret;
}

// We rely on the rlimit to enforce the memlimit (we once made an expensive
// call (getrusage() or GNU libc's atrocious mallinfo()) each allocation!)
static int
check_alloc_req(size_t s,const char *name){
	if(s == 0){
		bitch("Existential error: %s used *alloc(0)\n",name);
		return -1;
	}
	++allocs_reqd;
	if(poisoned_idx == 0){
		nag("Oh shit, there's a horse in the allocator\n");
		goto err;
	}
	if(poisoned_idx > 0){
		--poisoned_idx;
	}
	return 0;

err:
	errno = ENOMEM;
	return -1;
}

// Initializes returned memory to 0.
void *Malloc(const char *name,size_t s){
	void *ret = NULL;
	int infail = 0;

	pthread_mutex_lock(&memlock);
	if(check_alloc_req(s,name) == 0){
		if( (ret = malloc(s)) ){
			++allocs_used;
		}else{
			++allocs_fail;
			infail = 1;
		}
	}
	pthread_mutex_unlock(&memlock);
	if(ret){
		// nag("%.50s: %zu @ %p\n",name,s,ret);
	}else{
		if(infail){
			bitch("%.50s: %zu failed\n",name,s);
		}
		errno = ENOMEM;
	}
	return ret;
}

void *Realloc(const char *name,void *orig,size_t s){
	void *ret = NULL;
	int infail = 0;

	pthread_mutex_lock(&memlock);
	if(check_alloc_req(s,name) == 0){
		if( (ret = realloc(orig,s)) ){
			allocs_used += (orig == NULL);
		}else{
			++allocs_fail;
			infail = 1;
		}
	}
	pthread_mutex_unlock(&memlock);
	if(ret){
		// nag("%.50s: %zu @ %p\n",name,s,ret);
	}else{
		if(infail){
			bitch("%.50s: %zu failed\n",name,s);
		}
		errno = ENOMEM;
	}
	return ret;
}
	
// returns 0 for failure, otherwise a scaled allocation request size
static size_t
size_palloc_req(unsigned p,const char *name){
	size_t left,reqsize;

	nag("Requested %u%% of active memory for %s\n",p,name);
	// we can't take a percentage prior to establishing free ram
	if(memory_usage_limit == 0){
		return 0;
	}
	left = memory_usage_limit - mem_used();
	nag("Memory cap details %zu bytes left\n",left);
	reqsize = left / 100;
	reqsize *= p;
	left /= 10;
	left *= 9;
	if(reqsize > left){
		reqsize = left;
		nag("%%-alloc capped at %zu bytes for sanity\n",reqsize);
	}
	return reqsize;
}

// Allocate a percentage of available memory for the object listed.  Totally
// non-exact. The percentage p must satisfy 0 < p <= 100. Initializes returned
// memory to 0.
void *Palloc(const char *name,size_t osiz,unsigned *num,unsigned p){
	void *ret = NULL;
	size_t rsiz;

	if(p <= 0 || p > 100){
		bitch("Invalid allocation %u%% for %s\n",p,name);
		return NULL;
	}
	pthread_mutex_lock(&memlock);
	++allocs_reqd;
	if((rsiz = size_palloc_req(p,name)) < osiz){
		++allocs_fail;
	}else{
		rsiz -= rsiz % osiz;
		if((ret = malloc(rsiz)) == NULL){
			++allocs_fail;
		}else{
			++allocs_used;
		}
	}
	pthread_mutex_unlock(&memlock);
	if(ret == NULL){
		bitch("%%-alloc failed for %zu bytes for %s\n",rsiz,name);
	}else{
		*num = rsiz / osiz;
		nag("%s: %zu(%p) ok\n",name,rsiz,ret);
		nag("%u%% -> %zub (%u %zub %s)\n",p,rsiz,*num,osiz,name);
	}
	return ret;
}

static inline void
deepestfree(void *obj){
	pthread_mutex_lock(&memlock);
		--allocs_used;
	pthread_mutex_unlock(&memlock);
	free(obj);
}

void deeperfree(logctx *lc){ // only for use by free_logctx()!
	if(lc){
		deepestfree(lc);
	}
}

void Deepfree(const char *fname __attribute__ ((unused)),void *obj){
	if(obj == NULL){
		return;
	}else{
		// nag("Freeing %p for %s\n",obj,fname);
		deepestfree(obj);
	}
}

void *Mmalloc(const char *name,size_t len){
	const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	const int prot = PROT_READ | PROT_WRITE;
	const int fd = -1;

	void *ret = MAP_FAILED;
	int err = ENOMEM;

	pthread_mutex_lock(&memlock);
	if(check_alloc_req(len,name) == 0){
		err = 0;
		if((ret = mmap(0,len,prot,flags,fd,(off_t)0)) == MAP_FAILED){
			err = errno;
		}
	}
	if(ret != MAP_FAILED){
		++allocs_used;
	}else{
		++allocs_fail;
	}
	pthread_mutex_unlock(&memlock);
	if(ret == MAP_FAILED){
		errno = err;
		moan("%s: couldn't mmap %zu bytes\n",name,len);
	}else{
		// nag("%s: %zu(%p) ok\n",name,len,ret);
	}
	errno = err;
	return ret;
}

int Mfree(void *start,size_t len){
	int ret;

	pthread_mutex_lock(&memlock);
	if((ret = munmap(start,len)) == 0){
		--allocs_used;
	}
	pthread_mutex_unlock(&memlock);
	if(ret){
		moan("Couldn't munmap %zub at %p\n",len,start);
	}else{
		// nag("Unmapped %zub at %p\n",len,start);
	}
	return ret;
}

int track_allocation(const char *name){
	int ret;

	pthread_mutex_lock(&memlock);
	if((ret = check_alloc_req(1,name)) == 0){
		++allocs_used;
	}else{
		errno = ENOMEM;
		++allocs_fail;
	}
	pthread_mutex_unlock(&memlock);
	return ret;
}

void track_deallocation(void){
	pthread_mutex_lock(&memlock);
		--allocs_used;
	pthread_mutex_unlock(&memlock);
}

void track_failloc(void){
	pthread_mutex_lock(&memlock);
		++allocs_fail;
	pthread_mutex_unlock(&memlock);
}

// After n >= 0 successful allocations, make all fail. Hilarity ensues.
void failloc_on_n(intmax_t n){
	pthread_mutex_lock(&memlock);
	if(poisoned_idx >= 0 && n < 0){
		nag("Applied theriac to the allocator\n");
	}
	poisoned_idx = n; // silent on poison; there'll be many
	pthread_mutex_unlock(&memlock);
}

void *mremap_and_truncate(int fd,void *oldaddr,size_t oldlen,size_t newlen,
					int prot,int flags){
	if(fd >= 0){
		if(newlen >= oldlen){
			// See FreeBSD mmap(2) notes regarding MAP_NOSYNC and
			// the fragmentation problems related to ftruncate() on
			// such maps...
			static const char zerobuf[1];

			if(Lseek(fd,newlen - 1,SEEK_SET) < 0){
				return MAP_FAILED;
			}
			if(newlen > oldlen){
				if(Write(fd,zerobuf,sizeof(zerobuf)) != sizeof(zerobuf)){
					return MAP_FAILED;
				}
			}
		}else if(Ftruncate(fd,newlen)){
			nag("Oldlen on failed ftruncate: %zu @ %p\n",oldlen,oldaddr);
			return MAP_FAILED;
		} 
	}
	if(oldaddr == NULL){
		void *ret;

		if(track_allocation("mremap")){ // sets errno on error
			return MAP_FAILED;
		}
		if((ret = Mmap(NULL,newlen,prot,flags,fd,0)) == MAP_FAILED){
			track_failloc();
			track_deallocation();
		}
		return ret;
	}
	return Mremap(fd,oldaddr,oldlen,newlen,prot,flags);
}

int mremap_munmap(void *addr,size_t len){
	int ret;

	if((ret = Munmap(addr,len)) == 0){
		track_deallocation();
	}
	return ret;
}
