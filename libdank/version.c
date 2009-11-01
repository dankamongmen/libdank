#ifndef VERSION
	#error "VERSION must be defined!"
#else
#include <libdank/version.h>

// Syntax minutae require keeping these three lines distinct!
const char LIBDANK_REVISION[] =
	#include <libdank/ersatz/svnrev.h>
;

const char Libdank_Version[] = VERSION;
const char Libdank_Compiler[] = "gcc-" __VERSION__;
const char Libdank_Build_Date[] = __TIME__ " " __DATE__;
#endif
