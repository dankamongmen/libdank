#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <libdank/utils/maxfds.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>

// there are about 40,006 different ways to find the maximum file
// descriptor one can access.  as we demux off fd's everywhere, we must
// know the max we can get.

// RLIMIT_NOFILE (getrlimit(RLIMIT_NOFILE,&rlims))
// no good -- we can be handed a file descriptor via dup or send that is
// higher than this, or create one and drop the rlimit

// getdtablesize()
// nope, tries to use RLIMIT_NOFILE

// _POSIX_OPEN_MAX -> sysconf(_SC_OPEN_MAX)
// seems accurate, but watch for -1/errno == 0 "unspecified" return

// NR_OPEN
// defined different ways in linux/limits.h and linux/fs.h, very large
// in the latter (2 ^ 20).  makes me nervous!

// i think this is really the max number, not maximum fd...we may be
// able to drop the + 1 in determine_max_fds, probably better for
// memory...and more accurate.  XXX
static int max_fd;

int determine_max_fds(void){
	struct rlimit rlim;
	int rlimit;
	long sc;

	if(max_fd){
		return max_fd;
	}
	errno = 0;
	if((max_fd = getdtablesize()) > 0){
		nag("Using %d from getdtablesize() as maxfd + 1\n",max_fd);
		return max_fd;
	}
	if((sc = Sysconf(_SC_OPEN_MAX)) <= 0){
		if(errno){
			moan("Couldn't get system conf OPEN_MAX\n");
		}else{
			bitch("Couldn't get system conf OPEN_MAX\n");
		}
	}
	if(getrlimit(RLIMIT_NOFILE,&rlim)){
		moan("Couldn't get rlimit RLIMIT_NOFILE\n");
		rlimit = -1;
	}else{
		rlimit = rlim.rlim_max;
	}
	nag("sysconf(): %ld getrlimit(): %d\n",sc,rlimit);
	max_fd = sc;
	nag("Using the value %d as max fd\n",max_fd);
	return max_fd + 1;
}

void *allocate_per_possible_fd(const char *name,size_t objsize){
	size_t req;

	req = determine_max_fds() * objsize;
	return Malloc(name,req);
}

int sanity_check_fd(int fd){
	if(fd < 0 || fd > max_fd){
		bitch("Invalid file descriptor: %d (max: %d)\n",fd,max_fd);
		return -1;
	}
	return fd;
}

int safe_socket(int domain,int type,int protocol){
	int ret;

	if((ret = socket(domain,type,protocol)) < 0){
		moan("Couldn't get a %d/%d/%d socket\n",
				domain,type,protocol);
		return -1;
	}
	if(sanity_check_fd(ret) < 0){
		Close(ret);
		return -1;
	}
	return ret;
}
