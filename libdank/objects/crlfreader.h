#ifndef OBJECTS_CRLFREADER
#define OBJECTS_CRLFREADER

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/uio.h>

#define CRLF "\xd\xa"

typedef struct crlf_reader {
	struct iovec iv;
	// internal to crlf_reader; don't touch
	char *buf;
	int eof,readreq;
	size_t count,total,base,examined;
} crlf_reader;

// Call before using a crlf_reader the first time, 0 on success
int init_crlf_reader(crlf_reader *);

// Call to provide a buffer, as opposed to allocating a new one
int setup_crlf_reader(crlf_reader *,void *,size_t);

// SMTP/HTTP/ICAP work using the "line" as its basic structure. This object
// provides line buffering (almost: see [0]) on such a stream, suitable for use
// with non-blocking I/O.
//
// MOREDATA indicates that more data was read than occupies one line. Thus,
// there's still data in the buffer, but may not be any on the wire. Before
// poll() or select() is called again to look for a read, read_crlf_line()
// should again be invoked. SHORTLINE indicates that a non-empty string of
// which CRLF was not a suffix was followed by EOF.
//
// [0]. RFC 2821 2.3.7 sets no definite limits on length; lines end with a CRLF
// (0d0a). This would imply infinite buffering of reads necessitating the
// addition of CRLF_READ_LONGLINE.
typedef enum {
	CRLF_READ_SUCCESS,
	CRLF_READ_SHORTLINE,
	CRLF_READ_LONGLINE,
	CRLF_READ_MOREDATA,
	CRLF_READ_EOF,
	CRLF_READ_NBLOCK,
	CRLF_READ_SYSERR,
} crlf_read_res;

// Read the next CRLF-terminated line, or up to MAX_LINE_LENGTH of unterminated
// data. If the return value is SUCCESS, LONGLINE or MOREDATA, the line and its
// length (including the CRLF, aside from the LONGLINE case) are stored into iv
// (whose members are otherwise undefined). The iov_base's contents must be
// free()d in this case. On error, the reader still must be cleaned up.
crlf_read_res read_crlf_line(crlf_reader *,int);

void reset_crlf_reader(crlf_reader *);

#ifdef __cplusplus
}
#endif

#endif
