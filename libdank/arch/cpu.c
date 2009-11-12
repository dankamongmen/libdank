#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <libtorque.h>
#include <libdank/arch/cpu.h>
#include <libdank/arch/cpucount.h>
#include <libdank/objects/logctx.h>

static pthread_mutex_t cpuid_lock = PTHREAD_MUTEX_INITIALIZER;

static int
get_l1_dline(unsigned *lsize){
	static unsigned L1_DLINE_SIZE;
	int ret = 0;

	pthread_mutex_lock(&cpuid_lock);
	if(L1_DLINE_SIZE == 0){
		struct libtorque_ctx *torctx;

		if((torctx = libtorque_init()) == NULL){
			ret = -1;
		}else{
			L1_DLINE_SIZE = 64; //FIXME extract real L1 dcache lsize
			if(libtorque_stop(torctx)){
				ret = -1;
			}
		}
	}
	if(ret == 0 && lsize){
		*lsize = L1_DLINE_SIZE;
	}
	pthread_mutex_unlock(&cpuid_lock);
	return ret;
}

int id_cpu(void){
	return get_l1_dline(NULL);
}

size_t align_size(size_t s){
	unsigned lsize;

	if(get_l1_dline(&lsize) == 0){
		unsigned shmask = lsize - 1;

		if(s <= lsize){
			while(lsize / 2 >= s){
				lsize /= 2;
			}
			return lsize;
		}
		return ((s / lsize) + (unsigned)(!!(s & shmask))) * lsize;
	}
	bitch("Couldn't get shift bits/mask\n");
	return 0;
}
