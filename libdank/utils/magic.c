#include <libdank/utils/magic.h>
#include <libdank/objects/logctx.h>
#include <libdank/utils/memlimit.h>

// Approximate square root of a 32-bit unsigned integer with no branches. This
// does not honor x > y -> sqrt(x) > sqrt(y), and thus cannot be used for the
// normal ordering! It just gives something "close" to the square root. The
// result *will* always be <= n.
unsigned sqrtu32(uint32_t n){
	return n >> ((32 - nlz32(n)) / 2);
}

// We want the coprime of n that minimizes distance from sqrt(n), breaking ties
// by maximizing distance from 0 mod n [0]. Essentially, we are constructing a
// perfect linear congruential generator, parameterized on the modulus (see
// Knuth-3E vol2 p17-19). This allows a pseudorandom full cycle (a permutation)
// parameterized on the seed and our LCG, in O(1) time and space. w00t!
// To find it, we start factoring the modulus, retaining prime non-divisors.
// Any product of non-trivial powers of these non-factors is obviously a valid
// coprime. Likewise, factors of the modulus needn't be considered for the
// generator. Once we pass the point at which the sieve^2 >= modulus, begin
// sifting for an acceptable coprime; in the worst (pedantic) cases, we then
// select in O(n * primes(n)) having used O(primes(n)) temporary space.
//
// [0] actually, right now we go up to modulus - 1, then down to 1, from sqrt
uint32_t find_coprime(uintmax_t n){ 
	uint32_t *primes = NULL,ret = 0,cur;
	uintmax_t sqr = sqrtu32(n),modulus = n;
	unsigned curprimes;
	int *factor = NULL;

	if((primes = Malloc("primes vector",sizeof(*primes) * sqr)) == NULL){
		goto done;
	}
	if((factor = Malloc("factor vector",sizeof(*factor) * sqr)) == NULL){
		goto done;
	}
	curprimes = 0;
	for(cur = 2 ; cur <= sqr && n > 1; ++cur){
		unsigned prit;

		for(prit = 0 ; prit < curprimes ; ++prit){
			if(cur % primes[prit] == 0){
				break;
			}
		}
		if(prit < curprimes){
			continue;
		}
		primes[curprimes] = cur;
		if(n % cur == 0){
			do{
				++factor[curprimes];
				n /= cur;
			}while((n > 1) && (n % cur == 0));
		}
		++curprimes;
	}
	if(n > 1){
		primes[curprimes] = n;
		factor[curprimes] = 1;
		++curprimes;
		n = 1;
	}
	for(cur = sqr ; cur < modulus ; ++cur){
		for(n = 0 ; n < curprimes ; ++n){
			if(factor[n]){
				if(cur % primes[n] == 0){
					break;
				}
			}
		}
		if(n == curprimes){
			ret = cur;
			break;
		}
	}
	if(ret == 0){
		for(cur = sqr - 1 ; cur > 1 ; --cur){
			for(n = 0 ; n < curprimes ; ++n){
				if(factor[n]){
					if(cur % primes[n] == 0){
						break;
					}
				}
			}
			if(n == curprimes){
				ret = n;
				break;
			}
		}
	}
	if(ret == 0){
		ret = 1;
	}
	nag("Modulus %ju has an acceptable coprime at %u\n",modulus,ret);

done:
	if(ret == 0){
		bitch("Internal error: a coprime could not be generated for %ju\n",modulus);
		ret = 1;
	}
	Free(primes);
	Free(factor);
	return ret;
}

