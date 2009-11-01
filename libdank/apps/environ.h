#ifndef APPS_INIT_ENVIRON
#define APPS_INIT_ENVIRON

#ifdef __cplusplus
extern "C" {
#endif

typedef struct envset {
	char **values;
	const char **keys;
} envset;

int inspect_env(envset *,envset *);

#ifdef __cplusplus
}
#endif

#endif
