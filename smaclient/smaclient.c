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
 *
 * What I try to do ?!
 * ===================
 * (Small version of htop! Print & Sort a list process depend on 
 *  their consumption of physical memory.)
 * 1. Fetch all "proc.psinfo.rss" data. $ pminfo -t proc.psinfo.rss:
 *      resident set size (i.e. physical memory) of the process
 * 2. then sort all these data.
 * 3. print first _x_ them. as _x_ is the number of screen capacity rows.
 *      PID     == internal instance identifier
 *      MEM     == instance value
 *      COMM    == external instance identifier
 *
 * Challenges
 * ==========
 * 1. Sorting problem.
 *      I tried to use qsort(). but It's hard.
 *      Initially, I thought to make a big array and alloc _everything_
 *      but I think this not the best solution.
 *      Specially, after reading how pcp-atop impl this,
 *      but it's really complex ))
 *      pcp/src/atop/atopsar.c
 *
 * 2. Trivial issue. I think. I will solve it later.
 *      When I try to fetch data from use pmextractevalue() with kernel.uname
 *      It works with:
 *          kernel.uname.release
 *          kernel.uname.version
 *      I get Segmentation fault with:
 *          kernel.uname.sysname
 *          kernel.uname.machine
 *          kernel.uname.distro
 *
 * What's Next?
 * ============
 * 1. Reslove all FIXME tags. // double slash comments & /// comments
 * 2. replace any "fprintf"
 * 3. free any alloc memory
 * 4. qsort
 * 5. test the app with archive logs.
 * 6. ncurses main loop eats you cpu! https://stackoverflow.com/a/1222658
 *
*/
#include <stdlib.h> // qsort
#include <inttypes.h> // printf U64 for testing
#include "ncurses.h"
#include "pmapi.h"
#include "libpcp.h"
#include "pmnsmap.h"

#define numfetchs       100

pmLongOptions longopts[] = {
    PMAPI_GENERAL_OPTIONS,
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "pause", 0, 'P', 0, "pause between updates for archive replay" },
    PMAPI_OPTIONS_END
};

pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ | PM_OPTFLAG_BOUNDARIES,
    .short_options = PMAPI_OPTIONS "P",
    .long_options = longopts,
};

typedef struct {
    int         memrss[numfetchs];
    int         instaid[numfetchs];
    int         xxx;

} info_t;

static char        *sma_release;
static char        *sma_version;
static char        *sma_sysname;
static char        *sma_machine;
static char        *sma_distro;

/* ncurses functions */
void draw_borders(WINDOW *screen);
/* comparison function for qsort() */
static int comp(const void *a, const void *b);
void mypmSortInstances(pmResult *rp);


static void
general_info(void)
{
    static pmResult	*srp = NULL;	/* current */
    static int		numpmid_g;
    static pmID		*pmidlist_g;
    static pmDesc	*desclist_g;
    ///static char     *release;
    ///static char     *version;
    ///static char     *sysname;
    ///static char     *machine;
    ///static char     *distro;

    int			sts;
    int			i;
    pmAtomValue		atom;

    /* get numpmid */
    numpmid_g = sizeof(smaclient_init) / sizeof(char *);
    /* alloc memory     pmidlist */
    if ((pmidlist_g = (pmID *)malloc(numpmid_g * sizeof(pmidlist_g[0]))) == NULL) {
        fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
        exit(1);
    }

    /* alloc memory     desclist */
    if ((desclist_g = (pmDesc *)malloc(numpmid_g * sizeof(desclist_g[0]))) == NULL) {
        fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
        exit(1);
    }

    /* pmLookupName: find mertics you are intersted in */
    if ((sts = pmLookupName(numpmid_g, smaclient_init, pmidlist_g)) < 0) {
        printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
        for (i = 0; i < numpmid_g; i++) {
            if (pmidlist_g[i] == PM_ID_NULL)
                fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), smaclient_init[i]);
        }
        exit(1);
    }

    /* pmLookupDesc: fill desclist */
    for (i = 0; i < numpmid_g; i++) {
        if ((sts = pmLookupDesc(pmidlist_g[i], &desclist_g[i])) < 0) {
            fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                    pmGetProgname(), smaclient_init[i], pmIDStr(pmidlist_g[i]), pmErrStr(sts));
            exit(1);
        }
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid_g, pmidlist_g, &srp)) < 0) {
        fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    /*
     * But first ... is all the data present?
     */
    if (srp->vset[RELEASE]->numval <= 0 ||
            srp->vset[VERSION]->numval <= 0 ||
            srp->vset[SYSNAME]->numval <= 0 ||
            srp->vset[MACHINE]->numval <= 0 ||
            srp->vset[DISTRO]->numval <= 0) {
        /* do something ,, */
    } else {
        /* load average ... process all values, matching up the instances */
        pmExtractValue(srp->vset[RELEASE]->valfmt,
                &srp->vset[RELEASE]->vlist[i],
                desclist_g[RELEASE].type, &atom, PM_TYPE_STRING);
        sma_release = (char *) malloc(strlen(atom.cp));
        strcpy(sma_release, atom.cp);
        free(atom.cp);

        pmExtractValue(srp->vset[VERSION]->valfmt,
                &srp->vset[VERSION]->vlist[i],
                desclist_g[VERSION].type, &atom, PM_TYPE_STRING);
        sma_version = (char *)malloc(strlen(atom.cp));
        strcpy(sma_version, atom.cp);
        free(atom.cp);

        /* FIXME *********************************************************
         * Uncomment these lines to get Segmentation fault (core dumped)
         * why? I don't know =( ,,, I'll debug It later...
         *****************************************************************/

        ///pmExtractValue(srp->vset[SYSNAME]->valfmt,
                ///&srp->vset[SYSNAME]->vlist[i],
                ///desclist_g[SYSNAME].type, &atom, PM_TYPE_STRING);
        ///sma_sysname = (char *)malloc(strlen(atom.cp));
        ///strcpy(sma_sysname, atom.cp);
        ///free(atom.cp);
///
        ///pmExtractValue(srp->vset[MACHINE]->valfmt,
                ///&srp->vset[MACHINE]->vlist[i],
                ///desclist_g[MACHINE].type, &atom, PM_TYPE_STRING);
        ///sma_machine = (char *)malloc(strlen(atom.cp));
        ///strcpy(sma_machine, atom.cp);
        ///free(atom.cp);
///
        ///pmExtractValue(srp->vset[DISTRO]->valfmt,
                ///&srp->vset[DISTRO]->vlist[i],
                ///desclist_g[DISTRO].type, &atom, PM_TYPE_STRING);
        ///sma_distro = (char *)malloc(strlen(atom.cp));
        ///strcpy(sma_distro, atom.cp);
        ///free(atom.cp);

    }
    /* free very old result */
    pmFreeResult(srp);
}

static void
get_sample(info_t *ip)
{
    static pmResult	*crp = NULL;	/* current */
    static int		numpmid;
    static pmID		*pmidlist;
    static pmDesc	*desclist;
    ///static int      instaid[numfetchs];
    int			sts;
    int			i;
    pmAtomValue		atom;

    /* get numpmid */
    numpmid = sizeof(smaclient_sample) / sizeof(char *);
    /* alloc memory     pmidlist */
    if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
        fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
        exit(1);
    }
    /* alloc memory     desclist */
    if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
        fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
        exit(1);
    }
    /* pmLookupName: find mertics you are intersted in */
    if ((sts = pmLookupName(numpmid, smaclient_sample, pmidlist)) < 0) {
        printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
        for (i = 0; i < numpmid; i++) {
            if (pmidlist[i] == PM_ID_NULL)
                fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), smaclient_sample[i]);
        }
        exit(1);
    }

    /* pmLookupDesc: fill desclist */
    for (i = 0; i < numpmid; i++) {
        if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
            fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                    pmGetProgname(), smaclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
            exit(1);
        }
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &crp)) < 0) {
        fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    // FIXME: sort algorithm not work as I expected
   ///mypmSortInstances(crp); 

    for(i = 0; i < numfetchs; i++) { 
        pmExtractValue(crp->vset[MEMRSS]->valfmt, &crp->vset[MEMRSS]->vlist[i], desclist[MEMRSS].type, &atom, PM_TYPE_U64);
        ip->memrss[i]       = atom.ul; 
        ip->instaid[i]      = crp->vset[MEMRSS]->vlist[i].inst;
    }
    /* free very old result */
    pmFreeResult(crp);
}

int
main(int argc, char **argv)
{
    int			c, i;
    int			sts;
    int			samples;
    int			pauseFlag = 0;
    char		*source;
    info_t		info;
    //general_t		general;
    ///int         *b[numfetchs];
    ///char        *buf[numfetchs];
    ///char        **buff[numfetchs];
    WINDOW		*new;
    WINDOW		*logo;
    int 		parent_x, parent_y, new_x, new_y;
    int 		score_size = 3;
    int         titlerows = 1;

    setlinebuf(stdout);

    initscr();
    start_color();
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(1, COLOR_BLUE, COLOR_WHITE);
    curs_set(FALSE);

    getmaxyx(stdscr, parent_y, parent_x);
    new = newwin(parent_y - score_size, parent_x, 0, 0);
    logo = newwin(score_size, parent_x, parent_y - score_size, 0);

    setlinebuf(stdout);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
        switch (c) {
            case 'P':
                pauseFlag++;
                break;
            default:
                opts.errors++;
                break;
        }
    }

    if (pauseFlag && opts.context != PM_CONTEXT_ARCHIVE) {
        pmprintf("%s: pause can only be used with archives\n", pmGetProgname());
        opts.errors++;
    }
    if (opts.optind < argc - 1)
        opts.errors++;
    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
        sts = !(opts.flags & PM_OPTFLAG_EXIT);
        pmUsageMessage(&opts);
        exit(sts);
    }

    if (opts.context == PM_CONTEXT_ARCHIVE) {
        source = opts.archives[0];
    } else if (opts.context == PM_CONTEXT_HOST) {
        source = opts.hosts[0];
    } else {
        opts.context = PM_CONTEXT_HOST;
        source = "local:";
    }

    if ((sts = c = pmNewContext(opts.context, source)) < 0) {
        if (opts.context == PM_CONTEXT_HOST)
            fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
                    pmGetProgname(), source, pmErrStr(sts));
        else
            fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
                    pmGetProgname(), source, pmErrStr(sts));
        exit(1);
    }

    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
        pmflush();
        exit(1);
    }

    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
        opts.interval.tv_sec = 1;

    if (opts.context == PM_CONTEXT_ARCHIVE) {
        if ((sts = pmSetMode(PM_MODE_INTERP, &opts.start, (int)(opts.interval.tv_sec*1000 + opts.interval.tv_usec/1000))) < 0) {
            fprintf(stderr, "%s: pmSetMode failed: %s\n",
                    pmGetProgname(), pmErrStr(sts));
            exit(1);
        }
    }

    /* prime the results */
    if (opts.context == PM_CONTEXT_ARCHIVE) {
        get_sample(&info);	/* preamble record */
        get_sample(&info);	/* and into the real data */
    }
    else {
        get_sample(&info);	/* first pmFetch will prime the results */
        general_info();
    }

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    draw_borders(logo);

    get_sample(&info);

    while(samples == -1 || samples-- > 0) {
        getmaxyx(stdscr, new_y, new_x);
        /* - report format
           [PID]        Physical Mem    COMM
           [XXXXX]      XXXXXXX         /XXX/XXX/XXXXXX/XXXX/XX/XXXXX
           */
        mvwprintw(new, 0, 1, "[PID]");
        mvwprintw(new, 0, 16, "Physical Mem       ");
        mvwprintw(new, 0, 32, "COMM");
        wprintw(new, "xxx = %d", info.xxx);

        ///if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
           ///__pmtimevalSleep(opts.interval);

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

        /* claculate of screen rows capcaity and
         * fill the window according to this number */
        for(i = titlerows; i < new_y - score_size; i++) { 
            mvwprintw(new, i, 1, "[%d]  ", info.instaid[i]);
            mvwprintw(new, i, 16, "%d ", info.memrss[i]);
            mvwprintw(new, i, 32, "%s", "/placeholder/placeholder"); // external inst
        }

        /* FIXME: lots of reaundant funcions, optimize that later
         * May I need to merge them into one function */
        mvwprintw(logo, 1, 2, "RELEASE: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sma_release);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " VERSION: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sma_version);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " SYSNAME: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sma_sysname);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " MACHINE: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sma_machine);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " DISTRO: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sma_distro);
        wattroff(logo, COLOR_PAIR(1));

        /*  refresh all windows */
        wrefresh(new);
        wrefresh(logo);
    } // end main loop
    endwin();
    exit(0);
}

void
draw_borders(WINDOW *screen)
{
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


static int
comp(const void *a, const void *b)
{
    pmValue	*ap = (pmValue *)a;
    pmValue	*bp = (pmValue *)b;

    return bp->inst - ap->inst;
}

void
mypmSortInstances(pmResult *rp)
{
    int		i;

    for (i = 0; i < rp->numpmid; i++) {
        if (rp->vset[i]->numval > 1) {
            qsort(rp->vset[i]->vlist, rp->vset[i]->numval, sizeof(pmValue), comp);
        }
    }
}
