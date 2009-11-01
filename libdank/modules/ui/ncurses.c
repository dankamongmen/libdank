#include <term.h>
#include <curses.h>
#include <unistd.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/ui/color.h>
#include <libdank/modules/ui/ncurses.h>

int use_terminfo = 0;

int init_ncurses(void){
	if(isatty(STDOUT_FILENO)){
		int errret;

		// If NULL is passed, $TERM is used
		if(setupterm(NULL,STDOUT_FILENO,&errret) != OK){
			bitch("Couldn't set up terminfo(5) database (errret %d)\n",errret);
			return -1;
		}
		use_terminfo = 1;
	}
	return 0;
}

int stop_ncurses(void){
	return use_terminfo_defcolor();
}
