#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <libdank/utils/privs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/logctx.h>

static int
drop_privs_r(const char *username,const char *groupname,size_t buflen){
	struct passwd pwent,*pdb = NULL;
	struct group grent,*grp = NULL;
	uid_t uid,olduid = getuid();
	gid_t gid,oldgid = getgid();
	char buf[buflen];

	if(groupname){
		if(getgrnam_r(groupname,&grent,buf,sizeof(buf),&grp) || !grp){
			moan("Couldn't look up group %s\n",groupname);
			return -1;
		}
		gid = grp->gr_gid;
		if(setgid(gid)){
			moan("Couldn't setgid to group %d\n",gid);
			return -1;
		}
		if(getgid() != gid){
			bitch("getgid() returned %d, not %d\n",getgid(),gid);
			return -1;
		}
		if(getegid() != gid){
			bitch("getegid() returned %d, not %d\n",getegid(),gid);
			return -1;
		}
		nag("Dropped permissions to group %s (GID %d) from GID %d\n",groupname,gid,oldgid);
	}
	if(username){
		if(getpwnam_r(username,&pwent,buf,sizeof(buf),&pdb) || !pdb){
			moan("Couldn't look up username %s\n",username);
			return -1;
		}
		uid = pdb->pw_uid;
		if(setuid(uid)){
			moan("Couldn't setuid to user %d\n",uid);
			return -1;
		}
		if(getuid() != uid){
			bitch("getuid() returned %d, not %d\n",getuid(),uid);
			return -1;
		}
		if(geteuid() != uid){
			bitch("geteuid() returned %d, not %d\n",geteuid(),uid);
			return -1;
		}
		nag("Dropped permissions to user %s (UID %d) from UID %d\n",username,uid,olduid);
	}
	return 0;
}

// FIXME should take a list of supplementary groups to use with setgroups(2)
int drop_privs(const char *username,const char *groupname){
	long buflen;

	if((buflen = Sysconf(_SC_GETPW_R_SIZE_MAX)) < 0){
		nag("Couldn't look up getpwnam_r buffer len\n");
		// FIXME this won't allow an overwrite, but it might allow
		// a truncation. we ought size based off failed return value
		buflen = BUFSIZ;
	}
	return drop_privs_r(username,groupname,buflen);
}
