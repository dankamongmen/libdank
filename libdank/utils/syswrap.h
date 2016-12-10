#ifndef UTILS_SYSWRAP
#define UTILS_SYSWRAP

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include <libdank/ersatz/compat.h>
#ifdef LIB_COMPAT_FREEBSD
#include <sys/param.h>
#else
#include <sys/statfs.h>
#endif
#include <sys/mount.h>

// For fcntl(2) wrappers, look at fcntl.h. Pthreads wrappers are in threads.h.
// There is no wrapper for vfork(), since the child of a vfork() is not allowed
// to return from the function which called vfork().
struct pollfd;
struct sysinfo;

// Provide the function being wrapped, argument types, return value type (which
// must be equal to or contain the wrapper return value type), a predicate used
// to determine error, a printf-style format string to use when errors are
// detected, and arguments for that format string. The wrapped functions' 
// arguments may be referred to by phonetic Hebrew letters from, perversely,
// left to right, while the value to be returned may be referred to by 'ret'
// (These expressions will be evaluated using call-by-name semantics, so casts
// and even lexically-scoped function calls can be used). The predicate
// argument may be left blank to simply evaluate the return value in a logical
// (ie two-state) context...ummm, just look at the examples.
#define ONEARG_WRAPPER(fxn,argtype,rettype,pred,errfmtstr,...) \
static inline rettype libdank_##fxn##_wrapper(const char *caller,argtype aleph){ \
	rettype ret; \
	if((ret = fxn(aleph)) pred){ moanonbehalf(caller,errfmtstr "\n" ,##__VA_ARGS__); } \
	return ret; }

ONEARG_WRAPPER(close,int,int,,"Error closing %d",aleph)
#define Close(fd) libdank_close_wrapper(__func__,fd)
ONEARG_WRAPPER(fclose,FILE *,int,,"Error closing file pointer")
#define Fclose(fp) libdank_fclose_wrapper(__func__,fp)
ONEARG_WRAPPER(pclose,FILE *,int,,"Error pclosing pipe")
#define Pclose(pipefd) libdank_pclose_wrapper(__func__,pipefd)
ONEARG_WRAPPER(dup,int,int,< 0,"Error duping %d",aleph)
#define Dup(fd) libdank_dup_wrapper(__func__,fd)
ONEARG_WRAPPER(pipe,int *,int,,"Error creating pipe")
#define Pipe(pipefds) libdank_pipe_wrapper(__func__,pipefds)
ONEARG_WRAPPER(fflush,FILE *,int,,"Error flushing stream")
#define Fflush(fp) libdank_fflush_wrapper(__func__,fp)
ONEARG_WRAPPER(wait,int *,pid_t,< 0,"Error waiting on children")
#define Wait(status) libdank_wait_wrapper(__func__,status)
ONEARG_WRAPPER(closedir,DIR *,int,,"Error closing directory")
#define Closedir(dir) libdank_closedir_wrapper(__func__,dir)
ONEARG_WRAPPER(opendir,const char *,DIR *,== NULL,"Error opening directory %s",aleph)
#define Opendir(dname) libdank_opendir_wrapper(__func__,dname)
ONEARG_WRAPPER(fileno,FILE *,int,,"Error extracting fd")
#define Fileno(dir) libdank_fileno_wrapper(__func__,dir)
ONEARG_WRAPPER(system,const char *,int,,"Error executing command %s",aleph)
#define System(cmd) libdank_system_wrapper(__func__,cmd)
ONEARG_WRAPPER(chroot,const char *,int,,"Error chrooting to %s",aleph)
#define Chroot(cmd) libdank_chroot_wrapper(__func__,cmd)
ONEARG_WRAPPER(chdir,const char *,int,,"Error chdiring to %s",aleph)
#define Chdir(cmd) libdank_chdir_wrapper(__func__,cmd)
ONEARG_WRAPPER(unlink,const char *,int,,"Error unlinking %s",aleph)
#define Unlink(cmd) libdank_unlink_wrapper(__func__,cmd)
ONEARG_WRAPPER(shm_unlink,const char *,int,,"Error shm_unlinking %s",aleph)
#define Shm_unlink(cmd) libdank_shm_unlink_wrapper(__func__,cmd)
ONEARG_WRAPPER(mkstemp,char *,int,< 0,"Error mkstemping at %s",aleph)
#define Mkstemp(templ) libdank_mkstemp_wrapper(__func__,templ)
// readdir() annoyingly returns NULL both on error and to indicate
// end-of-directory; one must check (and thus also reset, prior to readdir()'s
// entry) errno on a NULL return.
ONEARG_WRAPPER(readdir,DIR *,struct dirent *,== NULL && errno,"Error reading directory")
#define Readdir(dir) (errno = 0, libdank_readdir_wrapper(__func__,dir))

#define TWOARG_WRAPPER(fxn,argtype,arg2type,rettype,pred,errfmtstr,...) \
static inline rettype libdank_##fxn##_wrapper(const char *caller,argtype aleph,arg2type bet){ \
	rettype ret; \
	if((ret = fxn(aleph,bet)) pred){ moanonbehalf(caller,errfmtstr "\n" ,##__VA_ARGS__); } \
	return ret; }

TWOARG_WRAPPER(listen,int,int,int,,"Error listening on %d (backlog: %d)",aleph,bet)
#define Listen(sd) libdank_listen_wrapper(__func__,sd,SOMAXCONN)
TWOARG_WRAPPER(dup2,int,int,int,< 0,"Error dup2ing %d onto %d",aleph,bet)
#define Dup2(fdaleph,fdbet) libdank_dup2_wrapper(__func__,fdaleph,fdbet)
TWOARG_WRAPPER(kill,pid_t,int,int,,"Error sending %s to %jd",strsignal(bet),(intmax_t)aleph)
#define Kill(pid,sig) libdank_kill_wrapper(__func__,pid,sig)
TWOARG_WRAPPER(getcwd,char *,size_t,char *,== NULL,"Couldn't get cwd in %zub",bet)
#define Getcwd(buf,blen) libdank_getcwd_wrapper(__func__,buf,blen)
TWOARG_WRAPPER(popen,const char *,const char *,FILE *,== NULL,"Couldn't open %s-type pipe to %s",bet,aleph)
#define Popen(cmd,typ) libdank_popen_wrapper(__func__,cmd,typ)
TWOARG_WRAPPER(link,const char *,const char *,int,,"Couldn't hardlink %s to %s",aleph,bet)
#define Link(cmd,typ) libdank_link_wrapper(__func__,cmd,typ)
TWOARG_WRAPPER(symlink,const char *,const char *,int,,"Couldn't symlink %s to %s",aleph,bet)
#define Symlink(cmd,typ) libdank_symlink_wrapper(__func__,cmd,typ)
TWOARG_WRAPPER(shutdown,int,int,int,,"Couldn't %s-shutdown %d",
	bet == SHUT_RD ? "recv" : bet == SHUT_WR ? "send" : "full",aleph)
#define Shutdown(sd,typ) libdank_shutdown_wrapper(__func__,cmd,typ)
TWOARG_WRAPPER(mkdir,const char *,int,int,,"Error creating dir at %s",aleph)
#define Mkdir(pid,sig) libdank_mkdir_wrapper(__func__,pid,sig)
TWOARG_WRAPPER(setpgid,pid_t,pid_t,int,,"Error setting pgid of %jd to %jd",(intmax_t)aleph,(intmax_t)bet)
#define Setpgid(pid,pgid) libdank_setpgid_wrapper(__func__,pid,pgid)
TWOARG_WRAPPER(munmap,void *,size_t,int,,"Error munmapping %zu at %p",bet,aleph)
#define Munmap(map,mlen) libdank_munmap_wrapper(__func__,map,mlen)
TWOARG_WRAPPER(truncate,const char *,off_t,int,,"Error truncating %s to %jd",aleph,(intmax_t)bet)
#define Truncate(path,len) libdank_truncate_wrapper(__func__,path,len)
TWOARG_WRAPPER(ftruncate,int,off_t,int,,"Error truncating %d to %jd",aleph,(intmax_t)bet)
#define Ftruncate(fd,len) libdank_ftruncate_wrapper(__func__,fd,len)
TWOARG_WRAPPER(stat,const char *,struct stat *,int,,"Error getting file state at %s",aleph)
#define Stat(path,s) libdank_stat_wrapper(__func__,path,s)
TWOARG_WRAPPER(fstat,int,struct stat *,int,,"Error getting file state from %d",aleph)
#define Fstat(fd,s) libdank_fstat_wrapper(__func__,fd,s)
TWOARG_WRAPPER(statfs,const char *,struct statfs *,int,,"Error getting FS state at %s",aleph)
#define Statfs(patfsh,s) libdank_statfs_wrapper(__func__,patfsh,s)
TWOARG_WRAPPER(fstatfs,int,struct statfs *,int,,"Error getting FS state from %d",aleph)
#define Fstatfs(fd,s) libdank_fstatfs_wrapper(__func__,fd,s)
TWOARG_WRAPPER(statvfs,const char *,struct statvfs *,int,,"Error getting VFS state at %s",aleph)
#define Statvfs(patvfsh,s) libdank_statvfs_wrapper(__func__,patvfsh,s)
TWOARG_WRAPPER(fstatvfs,int,struct statvfs *,int,,"Error getting VFS state from %d",aleph)
#define Fstatvfs(fd,s) libdank_fstatvfs_wrapper(__func__,fd,s)
TWOARG_WRAPPER(sigwait,const sigset_t *,int *,int,,"Error waiting on signals")
#define Sigwait(set,sig) libdank_sigwait_wrapper(__func__,set,sig)
TWOARG_WRAPPER(sigaddset,sigset_t *,int,int,,"Couldn't add signal %d (%s)",bet,strsignal(bet))
#define Sigaddset(set,sig) libdank_sigaddset_wrapper(__func__,set,sig)
TWOARG_WRAPPER(fdopen,int,const char *,FILE *,== NULL,"Couldn't %s-streamize %d",bet,aleph)
#define Fdopen(fd,mode) libdank_fdopen_wrapper(__func__,fd,mode)
TWOARG_WRAPPER(fopen,const char *,const char *,FILE *,== NULL,"Couldn't %s-open %s",bet,aleph)
#define Fopen(path,mode) libdank_fopen_wrapper(__func__,path,mode)
TWOARG_WRAPPER(getrlimit,int,struct rlimit *,int,,"Couldn't get rlimit %d",aleph)
#define Getrlimit(resource,rl) libdank_getrlimit_wrapper(__func__,resource,rl)
TWOARG_WRAPPER(getrusage,int,struct rusage *,int,,"Couldn't get %d-rusage",aleph)
#define Getrusage(who,ru) libdank_getrusage_wrapper(__func__,who,ru)
TWOARG_WRAPPER(setrlimit,int,const struct rlimit *,int,,"Couldn't set rlimit %d",aleph)
#define Setrlimit(resource,rl) libdank_setrlimit_wrapper(__func__,resource,rl)
TWOARG_WRAPPER(gettimeofday,struct timeval *,struct timezone *,int,,"Couldn't get time of day")
#define Gettimeofday(tv,tz) libdank_gettimeofday_wrapper(__func__,tv,tz)

// getpriority() can legitimately return the value -1, so we must reset and
// check errno...
TWOARG_WRAPPER(getpriority,int,int,int,< 0 && errno,"Couldn't get %d-%d priority",aleph,bet)
#define Getpriority(which,who) (errno = 0, libdank_getpriority_wrapper(__func__,which,who))

#define THREEARG_WRAPPER(fxn,argtype,arg2type,arg3type,rettype,pred,errfmtstr,...) \
static inline rettype libdank_##fxn##_wrapper(const char *caller,argtype aleph,\
				arg2type bet,arg3type gamel){ \
	rettype ret; \
	if((ret = fxn(aleph,bet,gamel)) pred){ moanonbehalf(caller,errfmtstr "\n" ,##__VA_ARGS__); } \
	return ret; }

/*
// readdir_r() is deprecated in glibc, and expected to be removed from POSIX.
THREEARG_WRAPPER(readdir_r,DIR *,struct dirent *,struct dirent **,int,,"Error reading directory")
#define Readdir_r(dir) libdank_readdir_r_wrapper(__func__,dir)
*/

#define ZEROARG_WRAPPER(fxn,rettype,pred,errfmtstr,...) \
static inline rettype libdank_##fxn##_wrapper(const char *caller){ \
	rettype ret; \
	if((ret = fxn()) pred){ moanonbehalf(caller,errfmtstr "\n" ,##__VA_ARGS__); } \
	return ret; }

ZEROARG_WRAPPER(fork,pid_t,< 0,"Forking error")
#define Fork() libdank_fork_wrapper(__func__)
ZEROARG_WRAPPER(tmpfile,FILE *,== NULL,"Error creating tmpfile in %s",P_tmpdir)
#define Tmpfile() libdank_tmpfile_wrapper(__func__)
ZEROARG_WRAPPER(getpagesize,int,<= 0,"Error getting pagesize")
// Deprecated. Use Sysconf(_SC_PAGE_SIZE) instead!
#define Getpagesize() libdank_getpagesize_wrapper(__func__)

int Fseek(FILE *,long,int);
int Open(const char *,int);
off_t Lseek(int,off_t,int);
ssize_t Read(int,void *,size_t);
ssize_t Write(int,const void *,size_t);
int Shm_open(const char *,int,mode_t);
int OpenCreat(const char *,int,mode_t);

int Poll(struct pollfd *,unsigned,int);
int Select(int,fd_set *,fd_set *,fd_set *,struct timeval *);

int Inet_pton(int,const char *,void *);
void *Mmap(void *,size_t,int,int,int,off_t)
       	__attribute__ ((warn_unused_result)) __attribute__ ((malloc));
void *Mremap_fixed(int,void *,size_t,size_t,void *,int,int);
void *Mremap(int,void *,size_t,size_t,int,int);

unsigned Alarm(unsigned);
unsigned Sleep(unsigned);
int Setpriority(int,int,int);
pid_t Waitpid(pid_t,int *,int);
int Daemon(const char *,int,int);
char *Setlocale(int,const char *);
int Execv(const char *,char * const []);
int Execvp(const char *,char * const []);
int Mount(const char *,const char *,int,void *);

int Sigaltstack(const stack_t *,stack_t *);
int Sigprocmask(int,const sigset_t *,sigset_t *);
int Sigaction(int,const struct sigaction *,struct sigaction *);

int Socket(int,int,int);
int Socketpair(int,int,int,int *);
int Recvmsg(int,struct msghdr *,int);
int Sendmsg(int,const struct msghdr *,int);
ssize_t Readv(int,const struct iovec *,int);
ssize_t Writev(int,const struct iovec *,int);
int Accept(int,struct sockaddr *,socklen_t *);
int Getsockopt(int,int,int,void *,socklen_t *);
int Bind(int,const struct sockaddr *,socklen_t);
int Accept4(int,struct sockaddr *,socklen_t *,int);
int Setsockopt(int,int,int,const void *,socklen_t);
int Connect(int,const struct sockaddr *,socklen_t);
int Getsockname(int,struct sockaddr *,socklen_t *);
int Sendto(int,const void *,size_t,int,const struct sockaddr *,socklen_t);

long Sysconf_named(const char *,int);
#define Sysconf(name) Sysconf_named(#name,name)

#ifdef MEMSTAT_METHOD_SYSINFO
int Sysinfo(struct sysinfo *);
#endif

#ifdef HAS_SYSCTLBYNAME
int Sysctlbyname(const char *,void *,size_t *,void *,size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif
