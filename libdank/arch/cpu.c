#include <stdint.h>
#include <pthread.h>
#include <libdank/arch/cpu.h>
#include <libdank/arch/cpucount.h>
#include <libdank/objects/logctx.h>

static int identified;
static unsigned L1_SHIFT_BITS,L1_SHIFT_MASK;
static pthread_mutex_t cpuid_lock = PTHREAD_MUTEX_INITIALIZER;

static int set_shift_bits(size_t cache_line_size);

typedef enum {
	CPUID_MAX_SUPPORT		=	0x00000000,
	CPUID_CPU_VERSION		=	0x00000001,
	CPUID_CACHE_TLB			=	0x00000002,
	CPUID_EXTENDED_MAX_SUPPORT	=	0x80000000,
	CPUID_EXTENDED_CPU_VERSION	=	0x80000001,
	CPUID_EXTENDED_CPU_NAME1	=	0x80000002,
	CPUID_EXTENDED_CPU_NAME2	=	0x80000003,
	CPUID_EXTENDED_CPU_NAME3	=	0x80000004,
	CPUID_EXTENDED_L1CACHE_TLB	=	0x80000005,
	CPUID_EXTENDED_L2CACHE		=	0x80000006,
} cpuid_class;

// By far, the best reference here is the IA-32 Intel Architecture Software
// Developer's Manual. http://faydoc.tripod.com/cpu/cpuid.htm isn't bad, nor
// is http://www.ee.nuigalway.ie/mirrors/www.sandpile.org/ia32/cpuid.htm.
//
// Uses all four primary general-purpose 32-bit registers (e[abcd]x), returning
// these in gpregs[0123]. We must preserve EBX ourselves in when -fPIC is used.
static inline void
cpuid(cpuid_class level,uint32_t *gpregs){
#ifdef __x86_64__
	__asm__ __volatile__(
		"cpuid\n\t" // serializing instruction
		: "=a" (gpregs[0]), "=b" (gpregs[1]),
		  "=c" (gpregs[2]), "=d" (gpregs[3])
		: "a" (level)
	);
#else
	__asm__ __volatile__(
		"pushl %%ebx\n\t"
		"cpuid\n\t" // serializing instruction
		"movl %%ebx,%%esi\n\t"
		"popl %%ebx\n\t"
		: "=a" (gpregs[0]), "=S" (gpregs[1]),
		  "=c" (gpregs[2]), "=d" (gpregs[3])
		: "a" (level)
	);
#endif
}

typedef enum {
	CPU_VENDER_INTEL,
	CPU_VENDER_AMD,
	CPU_VENDER_VIA,
	CPU_UNKNOWN
} cpu_vender;

typedef int (*cache_id_fxn)(uint32_t,size_t *);

typedef struct known_cpu_vender {
	cpu_vender vender_id;
	const char *signet;
	cache_id_fxn cachefxn;
} known_cpu_vender;

static int id_amd_cache(uint32_t,size_t *);
static int id_via_cache(uint32_t,size_t *);
static int id_intel_cache(uint32_t,size_t *);

// There's also: (Collect them all! Impress your friends!)
// 	" UMC UMC UMC" "CyriteadxIns" "NexGivenenDr"
// 	"RiseRiseRise" "GenuMx86ineT" "Geod NSCe by"
static const known_cpu_vender venders[] = {
	{	.signet = "GenuntelineI",
		.vender_id = CPU_VENDER_INTEL,
		.cachefxn = id_intel_cache,
	},
	{	.signet = "AuthcAMDenti",
		.vender_id = CPU_VENDER_AMD,
		.cachefxn = id_amd_cache,
	},
	{	.signet = "CentaulsaurH",
		.vender_id = CPU_VENDER_VIA,
		.cachefxn = id_via_cache,
	},
	{	.signet = NULL,
		.vender_id = CPU_UNKNOWN,
		.cachefxn = NULL,
	}
};

// vendstr should be 12 bytes corresponding to EBX, ECX, EDX post-CPUID
static const known_cpu_vender *
lookup_vender(const uint32_t *vendstr){
	const known_cpu_vender *vender;

	for(vender = venders ; vender->vender_id != CPU_UNKNOWN ; ++vender){
		if(memcmp(vendstr,vender->signet,sizeof(*vendstr) * 3) == 0){
			nag("Matched CPU signet: %s\n",vender->signet);
			break;
		}
	}
	return vender;
}

static const known_cpu_vender *
identify_cpuid(uint32_t *cpuid_max){
	const known_cpu_vender *vender;
	uint32_t gpregs[4];

	cpuid(CPUID_MAX_SUPPORT,gpregs);
	vender = lookup_vender(gpregs + 1);
	if(vender->vender_id == CPU_UNKNOWN){
		bitch("Unknown CPUID support: 0x%x 0x%x 0x%x 0x%x\n",gpregs[0],gpregs[1],gpregs[2],gpregs[3]);
		return NULL;
	}
	*cpuid_max = gpregs[0];
	return vender;
}

static const known_cpu_vender *
identify_extended_cpuid(uint32_t *cpuid_max){
	const known_cpu_vender *vender;
	uint32_t gpregs[4];

	cpuid(CPUID_EXTENDED_MAX_SUPPORT,gpregs);
	vender = lookup_vender(gpregs + 1);
	if(vender->vender_id == CPU_UNKNOWN){
		bitch("Unknown CPU extended support: %u %u %u %u\n",gpregs[0],gpregs[1],gpregs[2],gpregs[3]);
		return NULL;
	}
	*cpuid_max = gpregs[0];
	return vender;
}

typedef struct intel_dl1_descriptor {
	unsigned descriptor;
	size_t dl1_line_size;
} intel_dl1_descriptor;

static const intel_dl1_descriptor intel_dl1_descriptors[] = {
	{	.descriptor = 0x0a,
		.dl1_line_size = 32,
	},
	{	.descriptor = 0x0c,
		.dl1_line_size = 32,
	},
	{	.descriptor = 0x2c,
		.dl1_line_size = 64,
	},
	{	.descriptor = 0x60,
		.dl1_line_size = 64,
	},
	{	.descriptor = 0x66,
		.dl1_line_size = 64,
	},
	{	.descriptor = 0x67,
		.dl1_line_size = 64,
	},
	{	.descriptor = 0x68,
		.dl1_line_size = 64,
	},
	{	.descriptor = 0,
		.dl1_line_size = 0,
	}
};

static int
get_intel_clineb(unsigned descriptor,size_t *clineb){
	const intel_dl1_descriptor *id;

	for(id = intel_dl1_descriptors ; id->descriptor ; ++id){
		if(id->descriptor == descriptor){
			break;
		}
	}
	if(id->descriptor == 0){
		// nag("Unknown descriptor for Intel L1 dcache: 0x%x\n",descriptor);
		return 0;
	}
	nag("%zu-byte L1 dcache lines (descriptor 0x%x)\n",id->dl1_line_size,descriptor);
	if(*clineb){
		bitch("Got two results for Intel L1 dcache!\n");
		return -1;
	}
	*clineb = id->dl1_line_size;
	return 0;
}

static int
decode_intel_func2(uint32_t *gpregs,size_t *clineb){
	uint32_t mask;
	unsigned z;

	// Each GP register will set its MSB to 0 if it contains valid 1-byte
	// descriptors in each byte (save AL, the required number of calls).
	for(z = 0 ; z < 4 ; ++z){
		unsigned y;

		if(gpregs[z] & 0x80000000){
			continue;
		}
 		mask = 0xff000000;
		for(y = 0 ; y < 4 ; ++y){
			unsigned descriptor;

			if( (descriptor = (gpregs[z] & mask) >> ((3 - y) * 8)) ){
				if(get_intel_clineb(descriptor,clineb)){
					return -1;
				}
			}
			// Don't interpret bits 0..7 of EAX (AL in old notation)
			if((mask >>= 8) == 0x000000ff && z == 0){
				break;
			}
		}
	}
	return 0;
}

// Function 2 of Intel's CPUID -- See 3.1.3 of the CPUID Application Note
static int
extract_intel_func2(size_t *clineb){
	uint32_t gpregs[4],callreps;
	int ret;

	cpuid(CPUID_CACHE_TLB,gpregs);
	if((callreps = gpregs[0] & 0x000000ff) != 1){
		// FIXME must ensure that each subsequent level2 CPUID is on
		// the same CPU, probably using /dev/cpu/*/cpuid ala x86info
		bitch("Must CPUID Intel's L2 %u times\n",callreps);
		return -1;
	}
	nag("Must CPUID Intel's L2 %u time%s\n",callreps,callreps == 1 ? "" : "s");
	while((ret = decode_intel_func2(gpregs,clineb)) == 0){
		if(--callreps == 0){
			break;
		}
		cpuid(CPUID_CACHE_TLB,gpregs);
	}
	return ret;
}

static int
id_intel_cache(uint32_t maxlevel,size_t *clineb){
	if(maxlevel < CPUID_CACHE_TLB){
		return -1;
	}
	*clineb = 0;
	if(extract_intel_func2(clineb)){
		return -1;
	}
	if(*clineb == 0){
		bitch("Didn't get a valid Intel cache line size; PII?\n");
		return -1;
	}
	return 0;
}

static int
id_amd_cache(uint32_t maxlevel __attribute__ ((unused)),size_t *clineb){
	uint32_t maxexlevel,gpregs[4];
	const known_cpu_vender *cpu;

	if((cpu = identify_extended_cpuid(&maxexlevel)) == NULL){
		bitch("CPUID instruction couldn't ident extensions\n");
		return -1;
	}
	if(cpu->vender_id != CPU_VENDER_AMD){
		bitch("Extended CPUID check found %s\n",cpu->signet);
		return -1;
	}
	if(maxexlevel < CPUID_EXTENDED_L1CACHE_TLB){
		bitch("Maximum extended level: %u < %u\n",
			maxexlevel,CPUID_EXTENDED_L1CACHE_TLB);
		return -1;
	}
	// EAX/EBX: 2/4MB / 4KB TLB descriptors ECX: DL1 EDX: CL1
	cpuid(CPUID_EXTENDED_L1CACHE_TLB,gpregs);
	*clineb = gpregs[2] & 0x000000ff;
	return 0;
}

static int
id_via_cache(uint32_t maxlevel __attribute__ ((unused)),size_t *clineb){
	// XXX What a cheap piece of garbage, yeargh! VIA doesn't supply cache
	// line info via CPUID. VIA C3 Antaur/Centaur both use 32b. The proof
	// is by method of esoteric reference:
	// http://www.digit-life.com/articles2/rmma/rmma-via-c3.html
	*clineb = 32;
	return 0;
}

static int
cpuid_available(void){
	const unsigned long flag = 0x200000;
	unsigned long f1, f2;

	__asm__ volatile(
		"pushf\n\t"
		"pushf\n\t"
		"pop %0\n\t"
		"mov %0,%1\n\t"
		"xor %2,%0\n\t"
		"push %0\n\t"
		"popf\n\t"
		"pushf\n\t"
		"pop %0\n\t"
		"popf\n\t"
		: "=&r" (f1), "=&r" (f2)
		: "ir" (flag)
	);
	return ((f1 ^ f2) & flag) != 0;
}

static int
identify_cpu_locked(void){
	const known_cpu_vender *cpu;
	size_t cache_line_size;
	cpuid_class maxlevel;

	if(!cpuid_available()){
		bitch("No CPUID instruction available, aborting CPU detect\n");
		return -1;
	}
	nag("Examining CPU details via CPUID\n");
	if((cpu = identify_cpuid(&maxlevel)) == NULL){
		bitch("CPUID instruction couldn't identify processor\n");
		return -1;
	}
	if(cpu->cachefxn(maxlevel,&cache_line_size)){
		return -1;
	}
	return set_shift_bits(cache_line_size);
}

int id_cpu(void){
	int ret = 0;

	pthread_mutex_lock(&cpuid_lock);
	if(identified == 0){
		ret = identify_cpu_locked();
	}
	pthread_mutex_unlock(&cpuid_lock);
	return ret;
}

static int
get_shift_bits(unsigned *shbits,unsigned *shmask){
	int ret = -1;

	pthread_mutex_lock(&cpuid_lock);
	if(identified || (identify_cpu_locked() == 0)){
		*shbits = L1_SHIFT_BITS;
		*shmask = L1_SHIFT_MASK;
		nag("%u shift bits, & 0x%x (%d)\n",*shbits,*shmask,*shmask);
		ret = 0;
	}
	pthread_mutex_unlock(&cpuid_lock);
	return ret;
}

static int
set_shift_bits(size_t cache_line_size){
	unsigned shift_bits = 0;

	nag("Detected %zu-byte cache lines\n",cache_line_size);
	while(cache_line_size > 1){
		if(cache_line_size & 0x01){
			bitch("L1 dcache line size invalid\n");
			return -1;
		}
		cache_line_size >>= 1;
		++shift_bits;
	}
	if(shift_bits == 0){
		bitch("L1 dcache line was reported as 0b\n");
		return -1;
	}
	L1_SHIFT_BITS = shift_bits;
	L1_SHIFT_MASK = 1;
	while(--shift_bits){
		L1_SHIFT_MASK <<= 1;
		L1_SHIFT_MASK |= 0x1;
	}
	identified = 1;
	return 0;
}

size_t align_size(size_t s){
	unsigned shbits = 0,shmask = 0;

	if(get_shift_bits(&shbits,&shmask) == 0){
		if(s <= shmask + 1){
			++shmask;
			while(shmask / 2 >= s){
				shmask /= 2;
			}
			return shmask;
		}
		return ((s >> shbits) + (unsigned)(!!(s & shmask))) << shbits;
	}
	bitch("Couldn't get shift bits/mask\n");
	return 0;
}
