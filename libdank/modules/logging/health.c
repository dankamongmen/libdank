#include <sys/resource.h>
#include <libdank/utils/syswrap.h>
#include <libdank/objects/objustring.h>
#include <libdank/modules/logging/health.h>

#define STRINGIZE_RUSAGE_LONG(u,ru,elem) \
do { if((ru)->ru_##elem && printUString((u),"<"#elem">%ld</"#elem">",(ru)->ru_##elem) < 0){ return -1; } } while(0)
static int
stringize_rusage(ustring *u,const struct rusage *ru){
	STRINGIZE_RUSAGE_LONG(u,ru,maxrss);
	STRINGIZE_RUSAGE_LONG(u,ru,ixrss);
	STRINGIZE_RUSAGE_LONG(u,ru,idrss);
	STRINGIZE_RUSAGE_LONG(u,ru,isrss);
	STRINGIZE_RUSAGE_LONG(u,ru,minflt);	// linux support
	STRINGIZE_RUSAGE_LONG(u,ru,majflt);	// linux support
	STRINGIZE_RUSAGE_LONG(u,ru,nswap);
	STRINGIZE_RUSAGE_LONG(u,ru,inblock);
	STRINGIZE_RUSAGE_LONG(u,ru,oublock);
	STRINGIZE_RUSAGE_LONG(u,ru,msgsnd);
	STRINGIZE_RUSAGE_LONG(u,ru,msgrcv);
	STRINGIZE_RUSAGE_LONG(u,ru,nsignals);
	STRINGIZE_RUSAGE_LONG(u,ru,nvcsw);	// linux 2.6 support
	STRINGIZE_RUSAGE_LONG(u,ru,nivcsw);	// linux 2.6 support
	return 0;
}
#undef STRINGIZE_RUSAGE_LONG

static int
stringize_proc_rusage(ustring *u){
	struct rusage ru,ruc;

	if(Getrusage(RUSAGE_SELF,&ru)){
		return -1;
	}
	if(Getrusage(RUSAGE_CHILDREN,&ruc)){
		return -1;
	}
	ru.ru_maxrss += ruc.ru_maxrss;
	ru.ru_ixrss += ruc.ru_ixrss;
	ru.ru_idrss += ruc.ru_idrss;
	ru.ru_isrss += ruc.ru_isrss;
	ru.ru_minflt += ruc.ru_minflt;
	ru.ru_majflt += ruc.ru_majflt;
	ru.ru_nswap += ruc.ru_nswap;
	ru.ru_inblock += ruc.ru_inblock;
	ru.ru_oublock += ruc.ru_oublock;
	ru.ru_msgsnd += ruc.ru_msgsnd;
	ru.ru_msgrcv += ruc.ru_msgrcv;
	ru.ru_nsignals += ruc.ru_nsignals;
	ru.ru_nvcsw += ruc.ru_nvcsw;
	ru.ru_nivcsw += ruc.ru_nivcsw;
	if(stringize_rusage(u,&ru)){
		return -1;
	}
	return 0;
}

int stringize_health(ustring *u){
#define HEALTH_TAG "health"
	if(printUString(u,"<"HEALTH_TAG">") < 0){
		return -1;
	}
	if(stringize_proc_rusage(u)){
		return -1;
	}
	if(printUString(u,"</"HEALTH_TAG">") < 0){
		return -1;
	}
#undef HEALTH_TAG
	return 0;
}
