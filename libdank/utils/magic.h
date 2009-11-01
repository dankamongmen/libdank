#ifndef LIBDANK_UTILS_MAGIC
#define LIBDANK_UTILS_MAGIC

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Number of 1 bits in an unsigned 32-bit integer without branches
static inline unsigned
pop_count32(uint32_t n){
        n -= ((n >> 1) & 0x55555555);
        n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
        n = (n + (n >> 4)) & 0x0f0f0f0f;
        n += (n >> 8);
        n += (n >> 16);
        return n & 0x0000003f;
}

// Number of leading 0 bits in an unsigned 32-bit integer without branches
static inline unsigned
nlz32(uint32_t n){
        n |= (n >> 1);
        n |= (n >> 2);
        n |= (n >> 4);
        n |= (n >> 8);
        n |= (n >> 16);
        return pop_count32(~n);
}

// Find the log2 of a power of 2 in O(1), based off table lookup. This will
// return meaningless answers for anything but powers of 2 (ie, 0 or any uint
// x for which (x & (x - 1)) != 0).
static inline unsigned
uintlog2(uint64_t n){
#include <libdank/ersatz/magictables.h>
	return pow2table[n % sizeof(pow2table) / sizeof(*pow2table)];
}

unsigned sqrtu32(uint32_t);
uint32_t find_coprime(uintmax_t);

#ifdef __cplusplus
}
#endif

#endif
