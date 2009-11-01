#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <crosier/libcrosier.h>

__attribute__ ((noreturn)) static void
usage(const char *name){
	fprintf(stderr,"Usage: %s socketpath command [ cmddata ]\n",name);
	fprintf(stderr,"stdin will be read if no cmddata is specified\n");
	fprintf(stderr,"run netstat -lx to generate a list of socketpaths\n");
	exit(EXIT_FAILURE);
}

// FIXME we ought to ignore SIGPIPE

int main(int argc __attribute__ ((unused)),char * const *argv){
	const char *ctlsrvsocket,*xname,*cmd,*inname = "stdin";
	ssize_t octetsread = 0;
	int stderrno,sd;
	FILE *infp;

	xname = *argv;
	if((ctlsrvsocket = *++argv) == NULL || (cmd = *++argv) == NULL){
		usage(xname);
	}
	if(*++argv && strcmp(*argv,"-")){
		if((infp = fopen(*argv,"r")) == NULL){
			fprintf(stderr,"fopen(%s, \"r\"): %s\n",*argv,strerror(errno));
			return EXIT_FAILURE;
		}
		inname = *argv;
		if(*++argv){
			usage(xname);
		}
	}else{
		infp = stdin;
	}
	if((stderrno = fileno(stderr)) < 0){
		perror("fileno(stderr)");
		return EXIT_FAILURE;
	}
	if((sd = crosier_connect(ctlsrvsocket,stderrno)) < 0){
		return EXIT_FAILURE;
	}
	if(send_ctlrequest(sd,cmd,infp)){
		goto err;
	}
	if(infp != stdin && fclose(infp)){
		printf("fclose(%s): %s\n",inname,strerror(errno));
		goto err;
	}
	if((octetsread = recv_ctlreply(sd)) < 0){
		goto err;
	}
	if(close(sd)){
		fprintf(stderr,"Following success (%zdb) running %s:\nclose(%d): %s\n",
				octetsread,cmd,sd,strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	if(close(sd)){
		fprintf(stderr,"Following error running %s:\nclose(%d): %s\n",
				cmd,sd,strerror(errno));
	}
	return EXIT_FAILURE;
}
