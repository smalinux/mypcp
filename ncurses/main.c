#include <ncurses.h>

static void init_curses(void);
static void end_curses(void);

int main()
{
    init_curses();

    while(getch() != 'q')
    {
        printw("Hello World !!!");
        refresh();	
    }

    end_curses();

    return 0; // main();
}

static void init_curses()
{
    initscr();			/* Start curses mode 		  */
}

static void end_curses()
{
    endwin();			/* End curses mode		  */
}
