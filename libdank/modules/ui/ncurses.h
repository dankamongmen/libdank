#ifndef LIBDANK_MODULES_UI_NCURSES
#define LIBDANK_MODULES_UI_NCURSES

// FIXME this ought be exported const-only
extern int use_terminfo;

int init_ncurses(void);
int stop_ncurses(void);

#endif
