#include <string.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/events/evcore.h>
#include <libdank/modules/events/sources.h>

// The callback state associated with an event source.
typedef struct evsource {
	evcbfxn rxfxn;
	evcbfxn txfxn;
	void *cbstate;
	pthread_mutex_t lock;
} evsource;

evsource *create_evsources(unsigned n){
	evsource *evs;

	if( (evs = Malloc("evsources",sizeof(*evs) * n)) ){
		unsigned z;

		memset(evs,0,sizeof(*evs) * n);
		for(z = 0 ; z < n ; ++z){
			if(Pthread_mutex_init(&evs[z].lock,NULL)){
				destroy_evsources(evs,z);
				return NULL;
			}
		}
	}
	return evs;
}

// We need no locking here, because the only time someone should call
// setup_evsource is when they've been handed the file descriptor from the OS,
// and not handed it off to anything else which would register it. If it was
// already being used, it must have been removed from the event queue (by
// guarantees of the epoll/kqueue mechanisms), and thus no events exist for it.
void setup_evsource(evsource *evs,int n,evcbfxn rfxn,evcbfxn tfxn,void *v){
	evs[n].rxfxn = rfxn;
	evs[n].txfxn = tfxn;
	evs[n].cbstate = v;
}

int handle_evsource_read(evsource *evs,int n){
	if(evs[n].rxfxn){
		evs[n].rxfxn(n,evs[n].cbstate);
		return 0;
	}
	bitch("No rxfxn for %d\n",n);
	return -1;
}

int destroy_evsources(evsource *evs,unsigned n){
	int ret = 0;
	unsigned z;

	if(evs){
		for(z = 0 ; z < n ; ++z){
			ret |= Pthread_mutex_destroy(&evs[z].lock);
		}
		Free(evs);
	}
	return ret;
}
