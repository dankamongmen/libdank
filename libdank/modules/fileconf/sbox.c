#include <fcntl.h>
#include <sys/stat.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlversion.h>
#include <libdank/utils/fds.h>
#include <libdank/utils/text.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/threads.h>
#include <libdank/utils/memlimit.h>
#include <libdank/modules/fileconf/sbox.h>
#include <libdank/modules/ctlserver/ctlserver.h>

typedef int(*parsefxn)(FILE **);

typedef struct config_data {
	FILE *fp;
	char *fn;
	void *data;
	size_t len;
	parsefxn parser;
} config_data;

static const char *TOPLEVEL_DIR;
static char mutable_toplevel[PATH_MAX];

static char *
create_config_fn(const char *name){
	const char *dirname = TOPLEVEL_DIR ? TOPLEVEL_DIR : "";
	char *fn;

	nag("Conffile: %s%s\n",dirname,name);
	if( (fn = Malloc("filename",strlen(dirname) + strlen(name) + 1)) ){
		strcpy(fn,dirname);
		strcat(fn,name);
	}
	return fn;
}

static char *
check_config_fn(const char *name){
	char *fn;

	if( (fn = create_config_fn(name)) ){
		int fd;

		if((fd = OpenCreat(fn,O_CREAT,S_IRUSR | S_IWUSR)) < 0){
			Free(fn);
			return NULL;
		}
		if(Close(fd)){
			Free(fn);
			return NULL;
		}
	}
	return fn;
}

config_data *open_config(const char *name){
	config_data *tmp;

	if((tmp = Malloc(name,sizeof(*tmp))) == NULL){
		return NULL;
	}
	memset(tmp,0,sizeof(*tmp));
	if(name){
		if((tmp->fn = check_config_fn(name)) == NULL){
			goto err;
		}
	}else{
		nag("Using stdin for configuration\n");
	}
	return tmp;

err:
	Free(tmp);
	return NULL;
}

static int
lock_and_load_file(config_data *cfg,int flags){
	int fd;

	if((fd = OpenCreat(cfg->fn,flags | O_CREAT,0600)) < 0){
		return -1;
	}
	if((cfg->fp = Fdopen(fd,"rb")) == NULL){
		Close(fd);
		return -1;
	}
	return 0;
}

static int
unlock_and_unload_file(config_data *cfg){
	int ret = 0;

	ret |= Fclose(cfg->fp);
	return ret;
}

int parse_config(config_data *cfg,line_parser_cb lcb,void *arg){
	int ret = 0;

	if(lock_and_load_file(cfg,O_RDONLY)){
		return -1;
	}
	ret |= parser_byline(Fileno(cfg->fp),lcb,arg);
	ret |= unlock_and_unload_file(cfg);
	return ret;
}

int parse_wconfig(config_data *cfg,wline_parser_cb wlcb,void *arg){
	int ret = 0;

	if(lock_and_load_file(cfg,O_RDONLY)){
		return -1;
	}
	ret |= parser_bywline(cfg->fp,wlcb,arg);
	ret |= unlock_and_unload_file(cfg);
	return ret;
}

int parse_config_xmlfile(config_data *cfg,xmlDocPtr *doc){
	int ret = 0;

	if(lock_and_load_file(cfg,O_RDONLY)){
		return -1;
	}
	if((*doc = xmlReadFd(Fileno(cfg->fp),NULL,NULL,0)) == NULL){
		bitch("There was an error reading the XML file\n");
		ret = -1;
	}
	ret |= unlock_and_unload_file(cfg);
	return ret;
}

int reparse_config(line_parser_cb lcb,void *arg,FILE *fp){
	int ret = 0,fd;

	nag("Reparsing configuration using %p\n",fp);
	if((fd = Fileno(fp)) < 0){
		Fclose(fp);
		return -1;
	}
	ret |= parser_byline(fd,lcb,arg);
	ret |= Fclose(fp);
	return ret;
}

int reparse_xmlconf(xmlDocPtr *doc,FILE *fp){
	int ret = 0,fd;

	if((fd = Fileno(fp)) < 0){
		Fclose(fp);
		return -1;
	}
	if((*doc = xmlReadFd(fd,NULL,NULL,0)) == NULL){
		ret = -1;
	}
	ret |= Fclose(fp);
	return ret;
}

void free_config_data(config_data **cd){
	if(cd && *cd){
		Free((*cd)->fn);
		Free(*cd);
		*cd = NULL;
	}
}

int init_fileconf(const char *dir){
	int needsslash;

	LIBXML_TEST_VERSION; // FIXME is this safe here?
	xmlInitParser();
	if( (TOPLEVEL_DIR = dir) ){
		nag("Switching config dir to %s\n",dir);
		needsslash = (dir[strlen(dir) - 1] != '/');
		if(strlen(dir) + needsslash + 1 >= sizeof(mutable_toplevel)){
			bitch("Config directory too long: %s\n",dir);
			return -1;
		}
		strcpy(mutable_toplevel,dir);
		if(needsslash){
			strcat(mutable_toplevel,"/");
		}
		TOPLEVEL_DIR = mutable_toplevel;
	}
	return 0;
}

int stop_fileconf(void){
	xmlCleanupParser(); // FIXME is this safe here?
	return 0;
}
