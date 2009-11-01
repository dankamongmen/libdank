#include <libdank/utils/fds.h>
#include <cunit/cunit.h>
#include <libdank/utils/syswrap.h>
#include <libdank/utils/memlimit.h>
#include <libdank/objects/crlfreader.h>

#define CRLF "\xd\xa"

// Text to be read
static const char *textin[] = {
	CRLF,
	"0" CRLF,
	"0" "\xd\xa" "1",
	"0" "\xd\xa" "1" CRLF,
	NULL
};

// Corresponding sets of lines
static const char *text0[] = { CRLF, NULL };
static const char *text1[] = { "0" CRLF, NULL };
static const char *text2[] = { "0" CRLF, NULL };
static const char *text3[] = { "0" CRLF, "1" CRLF, NULL };

static const char **text[] = {
	text0, text1, text2, text3, NULL
};

static int
test_crlfreader(void){
	crlf_reader cr;
	int z;

	for(z = 0 ; textin[z] ; ++z){
		const char **t;
		int filedes[2];
		int const *rfd = &filedes[0],*wfd = &filedes[1];

		if(Pipe(filedes)){
			fprintf(stderr,"Couldn't open pipe.\n");
			return -1;
		}
		if(Writen(*wfd,textin[z],strlen(textin[z]))){
			fprintf(stderr,"Couldn't write %zu bytes to pipe.\n",strlen(textin[z]));
			Close(filedes[0]);
			Close(filedes[1]);
			return -1;
		}
		if(Close(*wfd)){
			fprintf(stderr,"Couldn't close write-pipe at %d.\n",*wfd);
			Close(*rfd);
			return -1;
		}
		if(init_crlf_reader(&cr)){
			Close(*rfd);
			return -1;
		}
		for(t = text[z] ; *t ; ++t){
			crlf_read_res res;

			res = read_crlf_line(&cr,*rfd);
			if(res == CRLF_READ_SUCCESS || res == CRLF_READ_MOREDATA || res == CRLF_READ_EOF){
				char *s = cr.iv.iov_base;

				if(strcmp(s,*t)){
					fprintf(stderr,"Expected %s, got %s\n",*t,s);
					reset_crlf_reader(&cr);
					Close(*rfd);
					return -1;
				}
				printf(" Correctly matched line %td of text %d.\n",t - text[z],z);
				Free(s);
			}else{
				fprintf(stderr," Expected %s, got %d\n",*t,res);
				reset_crlf_reader(&cr);
				Close(*rfd);
				return -1;
			}
		}
		reset_crlf_reader(&cr);
		Close(*rfd);
	}
	return 0;
}

const declared_test CRLFREADER_TESTS[] = {
	{	.name = "crlfreader",
		.testfxn = test_crlfreader,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	},
	{	.name = NULL,
		.testfxn = NULL,
		.expected_result = EXIT_TESTSUCCESS,
		.sec_required = 0, .mb_required = 0, .disabled = 0,
	}
};
