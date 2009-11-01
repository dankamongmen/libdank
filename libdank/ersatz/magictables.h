// Generated via tools/magictables on Fri Sep  4 21:00:40 EDT 2009

// For a word having exactly one bit set high, finding the position of that bit
// is equivalent to solving the logarithm base 2 of the word. The
// multiplicative group in any finite field is cyclic, and integers mod p where
// p is prime form a finite field. If we select a prime larger than our word
// size, for which 2 is a generator, an array sized in roughly linear
// proportion with the word length can be used as a precomputed lookup table
// for this solution. Otherwise (without hardware support) we require the naive
// O(N) shift-and-mask algorithm or O(lgN) divide-and-conquer shift-and-or
// algorithm. We can extend to 128 or 256 bits, for example, with 131 or 269.

static const uint8_t pow2table[67] = {
	127, 0, 1, 39, 2, 15, 40, 23, 3, 12, 16, 59, 41, 19, 24, 54, 
	4, 127, 13, 10, 17, 62, 60, 28, 42, 30, 20, 51, 25, 44, 55, 47, 
	5, 32, 127, 38, 14, 22, 11, 58, 18, 53, 63, 9, 61, 27, 29, 50, 
	43, 46, 31, 37, 21, 57, 52, 8, 26, 49, 45, 36, 56, 7, 48, 35, 
	6, 34, 33, 
};
