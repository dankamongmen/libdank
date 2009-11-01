#include <libdank/arch/timers.h>

uint_fast64_t x86_read_tsc(void){
	// FIXME on FreeBSD, use libpmc(3)'s pmc_allocate("tsc") combined with
	// pmc_read(). otherwise, fall back to assembly...?
	return 0;
}
