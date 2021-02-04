#include <ncurses.h>

static void init_curses(void);
static void end_curses(void);

int main()
{
    int i = 3;
    init_curses();

    while(getch() != 'q')
    {
        printw("Hello World !!!");
        if (i == 0) {
            refresh();	
            i = 3;
        }
    }

    end_curses();

    return 0; // main();
}

static void init_curses()
{
    initscr(); cbreak(); noecho();
}

static void end_curses()
{
    endwin();			/* End curses mode		  */
}

/*
 * ncurses
 *  * ncurses extensions
 *  * ncursesw - wide library
 *  * 
 *  *
 *
 *  <- lcurses_g ??
 */
