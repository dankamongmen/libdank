#include <libdank/utils/time.h>

void timeval_subtract(struct timeval *elapsed,const struct timeval *minuend,
			const struct timeval *subtrahend){
	*elapsed = *minuend;
	if(elapsed->tv_usec < subtrahend->tv_usec){
		int nsec = (subtrahend->tv_usec - elapsed->tv_usec) / 100000 + 1;

		elapsed->tv_usec += 1000000 * nsec;
		elapsed->tv_sec -= nsec;
	}
	if(elapsed->tv_usec - subtrahend->tv_usec > 1000000){
		int nsec = (elapsed->tv_usec - subtrahend->tv_usec) / 1000000;

		elapsed->tv_usec -= 1000000 * nsec;
		elapsed->tv_sec += nsec;
	}
	elapsed->tv_sec -= subtrahend->tv_sec;
	elapsed->tv_usec -= subtrahend->tv_usec;
}

// minuend \min"u*end\, n. [L. minuendus to be diminished, fr. minuere to
// lessen, diminish. See {Minish}.] (Arith.) In the process of subtraction, the
// number from which another number (the subtrahend) is to be subtracted, to
// find the difference.
intmax_t timeval_subtract_usec(const struct timeval *minuend,const struct timeval *subtrahend){
	struct timeval result;

	timeval_subtract(&result,minuend,subtrahend);
	return (intmax_t)result.tv_sec * 1000000 + result.tv_usec;
}
