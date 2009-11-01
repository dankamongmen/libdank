#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/netio.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/cmdstate.h>
#include <libdank/objects/objustring.h>

void init_cmd_state(cmd_state *cs,int sd,int errsd){
	memset(cs,0,sizeof(*cs));
	cs->errsd = errsd;
	cs->sd = sd;
}

int close_cmd_state(cmd_state *cs){
	int ret = 0;

	nag("Closing sd's %d and %d\n",cs->sd,cs->errsd);
	if(cs->sd >= 0){
		ret |= Close(cs->sd);
		cs->sd = -1;
	}
	if(cs->errsd >= 0){
		ret |= Close(cs->errsd);
		cs->errsd = -1;
	}
	return ret;
}

// FIXME speed this up; we do it this retarded way now
FILE *suck_socket_tmpfile(cmd_state *cs){
	unsigned tot = 0;
	FILE *tmp;

	if((tmp = Tmpfile()) == NULL){
		return NULL;
	}
	for( ; ; ){
		ssize_t s;
		char c;

		if((s = read(cs->sd,&c,1)) == 0){
			break;
		}else if(s < 0){
			if(errno == EINTR){
				continue;
			}
			moan("Couldn't read from socket %d\n",cs->sd);
			fclose(tmp);
			return NULL;
		}
		if(c == '\0'){
			break;
		}else{
			if(fwrite(&c,1,1,tmp) != 1){
				moan("Couldn't write 1b to tmpfile\n");
				fclose(tmp);
				return NULL;
			}
		}
		++tot;
	}
	if(fseek(tmp,0,SEEK_SET)){
		moan("Couldn't seek within tmpfile\n");
		fclose(tmp);
		return NULL;
	}
	nag("Wrote temporary file of %u bytes\n",tot);
	return tmp;
}

int dynsuck_socket_line(cmd_state *cs,char **buf){
	return read_socket_dynline(cs->sd,buf);
}
