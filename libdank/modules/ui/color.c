#include <term.h>
#include <libdank/objects/logctx.h>
#include <libdank/modules/ui/color.h>
#include <libdank/modules/ui/ncurses.h>

int use_terminfo_color(int ansicolor,int boldp){
	if(use_terminfo){
		const char *attrstr = boldp ? "bold" : "sgr0";
		const char *color,*attr;
		char *setaf;

		if((attr = tigetstr(attrstr)) == NULL){
			bitch("Couldn't get terminfo %s\n",attrstr);
			return -1;
		}
		putp(attr);
		if((setaf = tigetstr("setaf")) == NULL){
			bitch("Couldn't get terminfo setaf\n");
			return -1;
		}
		if((color = tparm(setaf,ansicolor)) == NULL){
			bitch("Couldn't get terminfo color %d\n",ansicolor);
			return -1;
		}
		putp(color);
	}
	return 0;
}

int use_terminfo_defcolor(void){
	// "default foreground color" according to http://bash-hackers.org/wiki/doku.php/scripting/terminalcodes
	// but not defined in ncurses.h -- likely not fully portable :( FIXME
	return use_terminfo_color(9,0);
}
