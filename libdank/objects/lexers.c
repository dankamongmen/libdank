#include <ctype.h>
#include <stdint.h>
#include <libdank/utils/parse.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/lexers.h>
#include <libdank/utils/memlimit.h>

// Ultra-paranoid overflow detection for per-digit lexing of unsigned integers
// 64 bits or less wide. Parameters:
//  bits: actual bit width of target integer [1..64]
//  augend: base to which we are adding, with max of (2 ^ bits) - 1
//  addend: ordinal value of lexed digit in the number system's representation,
//  		ie [0123456789ABCDEF] in hex maps to ordinals [0..15]
//  base: maximum ordinal value of one lexed digit plus one, ie hex -> 16
// Returns -1 on usage error, 0 on no overflow, 1 on overflow.
static int
would_overflow(unsigned bits,uintmax_t augend,uintmax_t addend,
							unsigned base){
	uintmax_t max;

	// FIXME key this off of sizeof(uintmax_t)
	if(bits > 64 || bits < 1){
		bitch("Bit widths supported: [1-64], got %u\n",bits);
		return -1;
	}
	if(base < 2 || base > 256){
		bitch("Bases supported: [2-256], got %u\n",base);
		return -1;
	}
	if(addend >= base){
		bitch("Addend >= base (%ju >= %u)\n",addend,base);
		return -1;
	}
	max = (~0ULL) >> (64 - bits);
	// nag("Max (%u bits): %llu >> %u = %llu\n",bits,~0ULL,64 - bits,max);
	// Determine if we'd overflow due to multiplying augend by base.
	if(augend > max / base){
		bitch("Overflows %u bit max %ju: %ju * %u\n",
				bits,max,augend,base);
		return 1;
	}
	augend *= base;
	// Determine if we'd overflow due to adding addend to augend * base
	// we know the * base is safe from above
	if(max - augend < addend){
		bitch("Overflows %u bit max %ju: %ju + %ju\n",
				bits,max,augend,addend);
		return 1;
	}
	// nag("%llu * %u + %llu is ok vs %llu\n",augend,base,addend,max);
	return 0;
}

static int
safely_add_oct(unsigned bits,uintmax_t *augend,int odig){
	uintmax_t addend;
	int ret;

	addend = odig - '0';
	if(addend >= 8){
		bitch("Digit not valid in octal: %c\n",odig);
		return -1;
	}
	if((ret = would_overflow(bits,*augend,addend,8))){
		return ret;
	}
	*augend *= 8;
	*augend += addend;
	// nag("Wanted %u-bit octal, got [%ju]\n",bits,*augend);
	return 0;
}

static int
safely_add_dec(unsigned bits,uintmax_t *augend,int digit){
	uintmax_t addend;
	int ret;

	addend = digit - '0';
	if((ret = would_overflow(bits,*augend,addend,10))){
		return ret;
	}
	*augend *= 10;
	*augend += addend;
	// nag("Wanted %u-bit decimal, got [%ju]\n",bits,*augend);
	return 0;
}

static int
safely_add_hex(unsigned bits,uintmax_t *augend,int xdig){
	uintmax_t addend;
	int ret;

	if(isdigit(xdig)){
		addend = xdig - '0';
	}else{
		xdig = tolower(xdig);
		addend = xdig - 'a' + 10;
	}
	if((ret = would_overflow(bits,*augend,addend,16))){
		return ret;
	}
	*augend *= 16;
	*augend += addend;
	// nag("Wanted %u-bit hexdecimal, got [%ju]\n",bits,*augend);
	return 0;
}

// We already saw a leading '0', but have not skipped it in buf (ie, *buf ==
// '0'). Zeros may not lead the actual number. So..
//   an 'x' or 'X' means skip one, reject 0, build a hex
//   '1'..'9' means build an octal
//   end of token means the value 0
//   anything else is rejected
static int
lex_uint_0helper(const unsigned char **buf,uintmax_t *val,unsigned bits){
	const unsigned char *start;

	// skip the leading zero
	start = (*buf)++;
	if(**buf == 'x' || **buf == 'X'){
		// preincrement to skip 'x'
		if(*++*buf == '0'){
			++*buf; // want to be past 0x0 for check
		}else{
			// haven't verified first (while), inc after safe add
			while(isxdigit(**buf)){
				if(safely_add_hex(bits,val,*(*buf)++)){
					goto err;
				}
			}
			return 0;
		}
	}else if(isdigit(**buf)){
		if(**buf == '0'){
			++*buf; // want to be past 00 for check
		}else{
			// preverified (do..while), inc after safe add
			do{
				if(safely_add_oct(bits,val,**buf)){
					goto err;
				}
			}while(isdigit(*++*buf));
			return 0;
		}
	}
	// this will set 0 for 0, 00, 0x0, reject other leading 0's
	if(**buf == '\0' || ispunct(**buf) || isspace(**buf)){
		*val = 0;
	}else{
		bitch("Leading zero in uint constant: %s\n",*buf);
		goto err;
	}
	return 0;

err:
	*buf = start;
	return -1;
}

// Lexing functions return -1 if they could not convert due to system error, 1
// if a parse error occured. Otherwise, they return 0, advance the double
// pointer, and store the result. The dpointer and result var are not molested
// if the result is invalid.
static int
lex_uint(const unsigned char **buf,uintmax_t *val,unsigned bits){
	const unsigned char *start = *buf,*numstart;
	uintmax_t v = 0;
	int ret = 1;

	// nag("Looking for %u bits in %s\n",bits,*buf);
	skip_whitespace(buf);
	numstart = *buf;
	if(*numstart == '0'){
		if(lex_uint_0helper(buf,val,bits)){
			goto err;
		}
	}else{
		while(isdigit(**buf)){
			if((ret = safely_add_dec(bits,&v,**buf))){
				goto err;
			}
			++*buf;
		}
		if(numstart == *buf){
			goto err;
		}
		*val = v;
	}
	// nag("Wanted %u-bit uint, got [%ju]\n",bits,*val);
	return 0;

err:
	bitch("Wanted %u-bit uint, got [%s]\n",bits,start);
	*buf = start;
	return ret;
}

static int
lex_uint_ashex(const unsigned char **buf,uintmax_t *val,unsigned bits){
	const unsigned char *start = *buf,*numstart;
	uintmax_t v = 0;
	int ret = 1;

	// nag("Looking for %u hex bits in %s\n",bits,*buf);
	skip_whitespace(buf);
	numstart = *buf;
	while(isxdigit(**buf)){
		if((ret = safely_add_hex(bits,&v,**buf))){
			goto err;
		}
		++*buf;
	}
	if(numstart == *buf){
		goto err;
	}
	*val = v;
	// nag("Wanted %u-bit uint, got [%llu]\n",bits,*val);
	return 0;

err:
	bitch("Wanted %u-bit uint, got [%s]\n",bits,start);
	*buf = start;
	return ret;
}

#define lex_uint_typewrapper(bits)					\
int lex_u##bits(const char **buf,uint##bits##_t *val){			\
	uintmax_t v = 0;						\
	int ret = 0;							\
									\
	/*if(CHAR_BIT * sizeof(*val) != bits){				\
		bitch("Type error! %u bits for %zub\n",			\
				bits,sizeof(*val));			\
		return -1;						\
	}*/								\
	if((ret = lex_uint((const unsigned char **)buf,&v,		\
				CHAR_BIT * sizeof(*val))) == 0){	\
		*val = v;						\
	}								\
	return ret;							\
}									\
									\
int lex_u##bits##_ashex(const char **buf,uint##bits##_t *val){		\
	uintmax_t v = 0;						\
	int ret = 0;							\
									\
	/* if(CHAR_BIT * sizeof(*val) != bits){				\
		bitch("Type error! %u bits for %zub\n",			\
				bits,sizeof(*val));			\
		return -1;						\
	}*/								\
	if((ret = lex_uint_ashex((const unsigned char **)buf,&v,	\
				CHAR_BIT * sizeof(*val))) == 0){	\
		*val = v;						\
	}								\
	return ret;							\
}

lex_uint_typewrapper(8)
lex_uint_typewrapper(16)
lex_uint_typewrapper(32)
lex_uint_typewrapper(64)
lex_uint_typewrapper(ptr)
lex_uint_typewrapper(max)

// Lexing functions return -1 if they could not convert due to system error, 1
// if a parse error occured. Otherwise, they return 0, advance the double
// pointer, and store the result. The dpointer and result var are not molested
// if the result is invalid.
static int
lex_sint(const unsigned char **buf,int64_t *val,unsigned bits){
	uint64_t uv;
	int neg;

	skip_whitespace(buf);
	if( (neg = (**buf == '-')) ){
		++*buf;
	}
	if(lex_uint(buf,&uv,bits - 1)){
		return -1;
	}
	*val = uv;
	if(neg){
		*val = -*val;
	}
	return 0;
}

#define lex_sint_typewrapper(bits)					\
int lex_s##bits(const char **buf,int##bits##_t *val){			\
	intmax_t v = 0;							\
	int ret = 0;							\
									\
	/*if(CHAR_BIT * sizeof(*val) != bits){				\
		bitch("Type error! %u bits for %zub\n",			\
				bits,sizeof(*val));			\
		return -1;						\
	}*/								\
	if((ret = lex_sint((const unsigned char **)buf,&v,		\
				CHAR_BIT * sizeof(*val))) == 0){ 	\
		*val = v;						\
	}								\
	return ret;							\
}

lex_sint_typewrapper(8)
lex_sint_typewrapper(16)
lex_sint_typewrapper(32)
lex_sint_typewrapper(64)
lex_sint_typewrapper(ptr)
lex_sint_typewrapper(max)
