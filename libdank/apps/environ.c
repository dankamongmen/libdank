#include <string.h>
#include <stdlib.h>
#include <libdank/apps/environ.h>
#include <libdank/objects/logctx.h>

static int
inspect_envset(envset *e){
	const char **key;
	char **value;

	if(e == NULL){
		return 0;
	}
	key = e->keys;
	value = e->values;
	while(*key){
		if((*value = getenv(*key)) == NULL){
			nag("Environment variable undefined: %s\n",*key);
		}else{
			nag("Environment variable %s=\"%s\"\n",*key,*value);
		}
		++value;
		++key;
	}
	return 0;
}

int inspect_env(envset *common,envset *app){
	int ret = 0;

	ret |= inspect_envset(common);
	ret |= inspect_envset(app);
	return ret;
}
