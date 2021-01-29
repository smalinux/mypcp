/*
                   ..                                       
                 .://-                                      
               .//://`         `                            
             `:/:`:/-        .//:-.                  `--:::`
            .//- .//`      .////////:-`     `     ``://///. 
           -//.  :/-       `////////////:--://::::///////`  
          :/:`  `//`    ..` -/////////////////////////:-`   
         :/:    -/:    .////://////////////////////:-`      
        :/:     //.  `.-/////////////////////////-`         
       -//`    .//   .//////////////////////////-           
      `//.     -/:    `:///////////////////////-            
      :/:      :/.      -://////////////////:.`             
     `//`     `//` `.-::- :////////////////.                
     :/:      .//-:/:-..``-///////////////`                 
    .//`      -///-`     -//////:////////`                  
   `//:        ``        ://///.`///////`                   
   ://.                 `////-   //////`                    
  .///                  .//-     -////`                     
  ://:                  --`      `--`                       
 `///.                                                      
 -///                                                       
 ://:                                                       
 ///-                                                       
.///.                                                       
-///.                                                       
////-                                                       
:///-                                                       
 .--`                                                       
*/
#include "ncurses.h"
#include "pmapi.h"
#include "libpcp.h"
#include "pmnsmap.h"

/* ncurses functions */
void draw_borders(WINDOW *screen);

int
main(int argc, char **argv)
{

    WINDOW		*new;
    WINDOW		*logo;
    int 		parent_x, parent_y, new_x, new_y;
    int 		score_size = 3;

    initscr();
    start_color();
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(1, COLOR_BLUE, COLOR_WHITE);
    wbkgd(new, COLOR_PAIR(2));
    bkgd(COLOR_PAIR(2));
    curs_set(FALSE);

    getmaxyx(stdscr, parent_y, parent_x);
    new = newwin(parent_y - score_size, parent_x, 0, 0);
    logo = newwin(score_size, parent_x, parent_y - score_size, 0);

    draw_borders(logo);
    while(1) {
        getmaxyx(stdscr, new_y, new_x);
        /* - report format
           CPU  Busy    Busy  Free Mem   Disk     Load Average
           Util   CPU    Util  (Mbytes)   IOPS    1 Min  15 Min
           X.XXX   XXX   X.XXX XXXXX.XXX XXXXXX  XXXX.XX XXXX.XX
           */
        if ( new_y != parent_y || new_x != parent_x ) {
            parent_x = new_x;
            parent_y = new_y;

            

            wresize(new, new_y - score_size, new_x);
            wresize(logo, score_size, new_x);
            mvwin(logo, new_y - score_size, 0);

            wclear(stdscr);
            wclear(new);
            wclear(logo);

            draw_borders(logo);
        }

        /* Start drawing after this line */
        mvwprintw(new, 1, 1, "  Title :"); // FIXME: I will merge this line and next line
        wprintw(new, "Value 1\n");
        
        mvwprintw(new, 2, 1, "  Title :"); // FIXME: I will merge this line and next line
        wprintw(new, "Value 1\n");
        
        mvwprintw(new, 3, 1, "  Title :"); // FIXME: I will merge this line and next line
        wprintw(new, "Value 1\n");
        
        mvwprintw(new, 4, 1, "Title :"); // FIXME: I will merge this line and next line
        wprintw(new, "Value 1\n");
        
        mvwprintw(new, 6, 1, "  Title :"); // FIXME: I will merge this line and next line
        wprintw(new, "Value 1\n");
        
        


        /* logo bar */
        char *cs = "Value";
        mvwprintw(logo, 1, 1, "Name: ");
        wattron(logo, COLOR_PAIR(1));
        mvwprintw(logo, 1, 10, "%s", cs);
        wattroff(logo, COLOR_PAIR(1));

        mvwprintw(logo, 1, 20, "Name: ");
        wattron(logo, COLOR_PAIR(1));
        mvwprintw(logo, 1, 30, "%s", cs);
        wattroff(logo, COLOR_PAIR(1));

        mvwprintw(logo, 1, 40, "Name: ");
        wattron(logo, COLOR_PAIR(1));
        mvwprintw(logo, 1, 50, "%s", cs);
        wattroff(logo, COLOR_PAIR(1));

        /*  refresh all windows */
        wrefresh(new);
        wrefresh(logo);
    } // end main loop
    endwin();
    exit(0);
}

void
draw_borders(WINDOW *screen) {
    int x, y, i;

    getmaxyx(screen, y, x);

    // 4 corners
    mvwprintw(screen, 0, 0, "+");
    mvwprintw(screen, y - 1, 0, "+");
    mvwprintw(screen, 0, x - 1, "+");
    mvwprintw(screen, y - 1, x - 1, "+");

    // sides
    for (i = 1; i < (y - 1); i++) {
        mvwprintw(screen, i, 0, "|");
        mvwprintw(screen, i, x - 1, "|");
    }

    // top and bottom
    for (i = 1; i < (x - 1); i++) {
        mvwprintw(screen, 0, i, "-");
        mvwprintw(screen, y - 1, i, "-");
    }
}
