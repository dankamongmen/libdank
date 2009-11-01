#include <stdio.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include <libdank/version.h>
#include <libdank/apps/init.h>
#include <libdank/ersatz/compat.h>
#include <libdank/utils/signals.h>
#include <libdank/modules/tracing/oops.h>
#include <libdank/modules/tracing/threads.h>
#include <libdank/modules/logging/logging.h>

typedef enum {
	APPLICATION_INIT,
	APPLICATION_RUNNING,
	APPLICATION_SHUTDOWN
} application_state_e;

const char *last_main_task = NULL;

static application_state_e astate = APPLICATION_INIT;

#define OOPS_STRINGIZER_MAX	4
static unsigned oops_stringizer_count;
static oops_stringizer oops_stringizers[OOPS_STRINGIZER_MAX];

// http://www.rdwarf.com/~kioh/haxorec30.jpg, enjoy
static const char
OOPS_MSG[] = "...What? Alarms? Good Lord, we're under electronic attack!";

static const char *
stringize_astate(application_state_e as){
	return as == APPLICATION_INIT		? "initialization" 	:
		as == APPLICATION_RUNNING	? "running"		:
		as == APPLICATION_SHUTDOWN	? "shutting down"	:
		"unknown";
}

static void
stringize_app(const app_def *app){
	if(!app || !app->appname){
		nag("No application data available\n");
	}else{
		nag("Standard app framework [%s]\n",app->appname);
	}
	nag("Version: %s rev %s (%s, %s)\n",
			Libdank_Version,LIBDANK_REVISION,
			Libdank_Compiler,Libdank_Build_Date);
}

static void
log_backtrace(void){
	void *array[20];
	char **strings;
	int size,i;

	if((size = backtrace(array,sizeof(array) / sizeof(*array))) <= 0){
		nag("Couldn't get backtrace pointers (Requires -g -ggdb -rdynamic, "
		"contraindicted by -O*, -fomit-frame-pointers)\n");
		return;
	}
	nag("Resolving %d stack frames (Requires -g -ggdb -rdynamic, "
		"contraindicted by -O*, -fomit-frame-pointers)\n",size);
	if((strings = backtrace_symbols(array,size)) == NULL){
		nag("Couldn't get backtrace symbols\n");
	}
	for(i = 0 ; i < size ; ++i){
		nag("%p %s\n",array[i],strings ? strings[i] : "(symbol lookup failed)");
	}
	free(strings);
}

void log_oops(int signum,const siginfo_t *si,const ucontext_t *uctx){
	void *ss_sp = &ss_sp;
	unsigned z;

	nag("%s\n",OOPS_MSG);
	nag("%s (%d) %s (%d)\n",strsignal(signum),signum,
		stringize_signal_code(si->si_code),si->si_code);
	stringize_app(application);

	// these signals give the address of the fault, which could be useful
	// given other information.
	if(signum == SIGILL || signum == SIGFPE || signum == SIGSEGV || signum == SIGBUS){
		nag("Location of fault: %p\n",si->si_addr);
	}

	if(uctx){
		const stack_t *stack = &uctx->uc_stack;

		nag("sp: %p oldsp: %p size: %zu flags: %s\n",
				stack->ss_sp,ss_sp,stack->ss_size,
				stack->ss_flags == SS_ONSTACK ? "SS_ONSTACK" :
				stack->ss_flags == SS_DISABLE ? "SS_DISABLE" :
				stack->ss_flags == 0 ? "none" : "??");
		ss_sp = stack->ss_sp;
	}

	nag("app period: %s\n",stringize_astate(astate));
	if(last_main_task){
		nag("last main task: %s\n",last_main_task);
	}
	log_tstk(ss_sp);
	log_backtrace();
	for(z = 0 ; z < oops_stringizer_count ; ++z){
		oops_stringizers[z]();
	}
}

void application_running(void){
	track_main("Completed initialization");
	timenag("Completed initialization\n");
	astate = APPLICATION_RUNNING;
}

void application_closing(void){
	track_main("Shutting down");
	timenag("Shutting down\n");
	astate = APPLICATION_SHUTDOWN;
}

void add_oops_stringizer(oops_stringizer os){
	if(oops_stringizer_count == OOPS_STRINGIZER_MAX){
		return;
	}
	oops_stringizers[oops_stringizer_count++] = os;
}
