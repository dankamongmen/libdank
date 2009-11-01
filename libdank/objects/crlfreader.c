#include <ctype.h>
#include <unistd.h>
#include <libdank/utils/string.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/logctx.h>
#include <libdank/objects/crlfreader.h>

static int
grow_buf(crlf_reader *cr){
	// Maximum number of bytes to read before branding a line as invalid.
	// RFC 282[12] put a strict max at 998 + CRLF for transmitters, but say
	// to go ahead and accept more if possible.
	const unsigned MAX_LINE_SIZE = 0x100000;
	size_t heap;
	void *tmp;

	if(cr->total >= MAX_LINE_SIZE){
		bitch("Refusing to grow %zdb past %ub for a single line\n",
				cr->total,MAX_LINE_SIZE);
		return -1;
	}
	// BUFSIZ on FreeBSD is only 512, to match disk sectors (Linux uses
	// 4096 to match pages). Perhaps the most natural value to use would be
	// our socket rx buffer size...for now, use 8192 hardcoded FIXME
	if((heap = cr->total + 8192) > MAX_LINE_SIZE){
		heap = MAX_LINE_SIZE;
	}
	if((tmp = Realloc("reader buffer",cr->buf,heap)) == NULL){
		Free(cr->buf);
		return -1;
	}
	cr->total = heap;
	cr->buf = tmp;
	return 0;
}

int init_crlf_reader(crlf_reader *cr){
	memset(cr,0,sizeof(*cr));
	if(grow_buf(cr)){
		return -1;
	}
	return 0;
}

int setup_crlf_reader(crlf_reader *cr,void *buf,size_t s){
	if(buf == NULL){
		return init_crlf_reader(cr);
	}
	memset(cr,0,sizeof(*cr));
	cr->total = s;
	cr->buf = buf;
	return 0;
}

void reset_crlf_reader(crlf_reader *cr){
	if(cr){
		Free(cr->buf);
		memset(cr,0,sizeof(*cr));
	}
}

static crlf_read_res
read_cr_data(crlf_reader *cr,int sd){
	char *readstart;
	ssize_t ret;
	size_t tlen;

	if(cr->eof){
		return CRLF_READ_EOF;
	}
	if(cr->count == cr->total){
		errno = 0;
		if(grow_buf(cr)){
			if(errno == ENOMEM){
				return CRLF_READ_SYSERR;
			}
			return CRLF_READ_LONGLINE;
		}
	}
	if(cr->count > cr->base){
		memmove(cr->buf,cr->buf + cr->base,cr->count);
	}else if(cr->base && cr->count){
		memcpy(cr->buf,cr->buf + cr->base,cr->count);
	}
	cr->base = 0;
	tlen = cr->total - cr->count;
	readstart = cr->buf + cr->count; // cr->base = 0 from above
	// nag("Reading %x from %d:%p + %x\n",tlen,sd,cr->buf,cr->count);
	cr->readreq = 0;
	while((ret = read(sd,readstart,tlen)) < 0){ // loop on EINTR
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			return CRLF_READ_NBLOCK;
		}else if(errno != EINTR){
			if(errno == ECONNRESET){
				nag("Remote side aborted connection on %d\n",sd);
			}else{
				moan("Couldn't read from socket %d\n",sd);
			}
			return CRLF_READ_SYSERR;
		}
	}
	if(ret == 0){
		cr->eof = 1;
		return CRLF_READ_EOF;
	}
	cr->count += ret;
	cr->readreq = ((size_t)ret == tlen);
	return CRLF_READ_SUCCESS;
}

// we have a more complicated issue due to the dual-character line ending
// (CRLF, 0x0d 0x0a).  no watching for comments, though, or trimming leading
// whitespace.
crlf_read_res read_crlf_line(crlf_reader *cr,int sd){
	char *linefeed = NULL;
	crlf_read_res res;

	for( ; ; ){
		char *examstart,*bufstart;
		size_t slen;

		// we need to read more if everything's been examined.
		if(cr->examined == cr->count){
			if((res = read_cr_data(cr,sd)) != CRLF_READ_SUCCESS){
				if(res != CRLF_READ_NBLOCK && cr->count){
					nag("Discarding %zu bytes\n",cr->count);
					if(res == CRLF_READ_EOF){
						return CRLF_READ_SHORTLINE;
					}
				}
				return res;
			}
		}
		// cr->count - cr->examined > 0. look for a CRLF
		slen = cr->count - cr->examined;
		bufstart = cr->buf + cr->base;
		examstart = bufstart + cr->examined;
		linefeed = strnchr(examstart,'\xa',slen);
		if(linefeed == NULL){
			cr->examined = cr->count;
		// special case; if we're at the beginning of a new
		// read, check the previous char *unless* there was no
		// previously-examined read, which is unsafe + useless
		}else if(cr->examined == 0 && linefeed == bufstart){
			cr->examined = 1;
		}else if(*(linefeed - 1) != '\xd'){
			cr->examined = linefeed - bufstart + 1;
		}else{
			char *newbuf;
			size_t len;

			// we want to retain length + 1 (for artificial
			// null byte) but return only length, so
			// allocate yet another + 1 after this.
			len = linefeed - bufstart + 1;
			if((newbuf = Malloc("recv copy",len + 1)) == NULL){
				return CRLF_READ_SYSERR;
			}
			memcpy(newbuf,bufstart,len);
			newbuf[len] = '\0';
			cr->iv.iov_base = newbuf;
			cr->iv.iov_len = len;
			cr->count -= len;
			cr->base += len;
			cr->examined = 0;
			if(cr->count || cr->eof){
				return CRLF_READ_MOREDATA;
			}else{
				return CRLF_READ_SUCCESS;
			}
		}
	}
}
