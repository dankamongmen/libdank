#include <sys/signal.h>
#include <libdank/utils/signals.h>

const char *stringize_signal_code(int code){
	return code == SI_USER 		? "userspace" 		:
#ifdef SI_KERNEL
		code == SI_KERNEL	? "kernel"		:
#endif
		code == SI_QUEUE	? "sigqueue"		:
		code == SI_TIMER	? "timer"		:
		code == SI_MESGQ	? "mesq state change"	:
		code == SI_ASYNCIO	? "async io completed"	:
#ifdef SI_SIGIO
		code == SI_SIGIO	? "queued sigio"	:
#endif
#ifdef SEGV_MAPERR
		code == SEGV_MAPERR	? "address not mapped"	:
#endif
#ifdef SEGV_ACCERR
		code == SEGV_ACCERR	? "no perms for address":
#endif
#ifdef ILL_BADSTK
		code == ILL_BADSTK	? "stack overflow"	:
#endif
#ifdef BUS_ADRALN
		code == BUS_ADRALN	? "invalid alignment"	:
#endif
#ifdef BUS_ADRERR
		code == BUS_ADRERR	? "invalid address"	:
#endif
		"unhandled code";
}
