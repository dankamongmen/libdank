#include <fcntl.h>
#include <libdank/utils/text.h>
#include <libdank/utils/string.h>
#include <libdank/utils/procfs.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/lexers.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>
#include <libdank/utils/lineparser.h>

#define PROCMOUNT "/proc" // FIXME determine this at runtime

static int
open_procpidfs_file(pid_t pid,const char *relpath){
	char path[PATH_MAX];

	if(snprintf(path,PATH_MAX,PROCMOUNT"/%d/%s",pid,relpath) >= PATH_MAX){
		bitch("Pathname was too long: "PROCMOUNT"/%d/%s\n",pid,relpath);
		return -1;
	}
	return Open(path,O_RDONLY);
}

int parse_pidprocfile_byline(const char *fn,pid_t pid,
			line_parser_cb lpcb,void *arg){
	int fd,ret = 0;

	if((fd = open_procpidfs_file(pid,fn)) < 0){
		return -1;
	}
	if(parser_byline(fd,lpcb,arg)){
		ret = -1;
	}
	if(Close(fd)){
		return -1;
	}
	return ret;
}

static int
cmdline_lineparser_callback(char *line,void *opaque_state){
	char **argv0 = opaque_state,*toke;

	if(*argv0){ // What if argv0 has a newline in it? FIXME
		return 0;
	}
	if((toke = carve_token(&line)) == NULL){
		bitch("Couldn't get a token from %s\n",line);
		return -1;
	}
	if((*argv0 = Strdup(toke)) == NULL){
		return -1;
	}
	return 0;
}

#define PROC_CMDLINE_FILE "cmdline"
char *procfs_cmdline_argv0(pid_t pid){
	char *ret = NULL;

	if(parse_pidprocfile_byline(PROC_CMDLINE_FILE,pid,
			cmdline_lineparser_callback,&ret)){
		if(ret){
			Free(ret);
			ret = NULL;
		}
	}
	return ret;
}
#undef PROC_CMDLINE_FILE

static int
open_procfs_file(const char *relpath){
	char path[PATH_MAX];

	if(snprintf(path,PATH_MAX,PROCMOUNT"/%s",relpath) >= PATH_MAX){
		bitch("Pathname was too long: "PROCMOUNT"/%s\n",relpath);
		return -1;
	}
	return Open(path,O_RDONLY);
}

int parse_procfile_byline(const char *fn,line_parser_cb lpcb,void *arg){
	int fd,ret = 0;

	if((fd = open_procfs_file(fn)) < 0){
		return -1;
	}
	if(parser_byline(fd,lpcb,arg)){
		ret = -1;
	}
	if(Close(fd)){
		return -1;
	}
	return ret;
}

struct taggeduint {
	const char *tag;
	uintmax_t *val;
};

static int
taggeduintcb(char *line,void *opaque){
	struct taggeduint *tuint = opaque;
	const char *val;

	if(strncmp(line,tuint->tag,strlen(tuint->tag))){
		return 0;
	}
	val = line + strlen(tuint->tag);
	if(lex_umax(&val,tuint->val)){
		return -1;
	}
	nag("Lexed %ju from tag %s\n",*tuint->val,tuint->tag);
	return 0;
}

int procfile_tagged_uint(const char *fn,const char *tag,uintmax_t *val){
	struct taggeduint tuint = { .tag = tag, .val = val, };

	return parse_procfile_byline(fn,taggeduintcb,&tuint);
}

int pidprocfile_tagged_uint(const char *fn,pid_t pid,const char *tag,uintmax_t *val){
	struct taggeduint tuint = { .tag = tag, .val = val, };

	return parse_pidprocfile_byline(fn,pid,taggeduintcb,&tuint);
}

struct matchedline {
	const char *line;
	unsigned *matches;
};

static int
matchlinecb(char *line,void *opaque){
	struct matchedline *mline = opaque;

	if(strcmp(line,mline->line)){
		return 0;
	}
	++*mline->matches;
	nag("Matched line %s\n",mline->line);
	return 0;
}

int procfile_match_line(const char *fn,const char *line,unsigned *matches){
	struct matchedline mline = { .line = line, .matches = matches, };

	return parse_procfile_byline(fn,matchlinecb,&mline);
}
