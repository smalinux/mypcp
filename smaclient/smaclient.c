// FIXME: -> // 
// FIXME: -> ///
// FIXME: fprintf
// FIXME: pmclient_sample -> rename
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
#include <stdlib.h>
#include <inttypes.h>
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
    struct timeval	timestamp;	/* last fetched time */
    float		cpu_util;	/* aggregate CPU utilization, usr+sys */
    int			peak_cpu;	/* most utilized CPU, if > 1 CPU */
    float		peak_cpu_util;	/* utilization for most utilized CPU */
    float		freemem;	/* free memory (Mbytes) */
    int			dkiops;		/* aggregate disk I/O's per second */
    float		load1;		/* 1 minute load average */
    float		load15;		/* 15 minute load average */
    int         memrss[numfetchs];
    int         instaid[numfetchs];

} info_t;

char        *release;
char        *version;
char        *sysname;
char        *machine;
char        *distro;

/* ncurses functions */
void draw_borders(WINDOW *screen);
/* comparison function for qsort() */
int cmpfunc(const void *a, const void *b);


static unsigned int	ncpu;

    static unsigned int
get_ncpu(void)
{
    /* there is only one metric in the pmclient_init group */
    pmID	pmidlist[1];
    pmDesc	desclist[1];
    pmResult	*rp;
    pmAtomValue	atom;
    int		sts;

    if ((sts = pmLookupName(1, pmclient_init, pmidlist)) < 0) {
        fprintf(stderr, "%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
        fprintf(stderr, "%s: metric \"%s\" not in name space\n",
                pmGetProgname(), pmclient_init[0]);
        exit(1);
    }
    if ((sts = pmLookupDesc(pmidlist[0], desclist)) < 0) {
        fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                pmGetProgname(), pmclient_init[0], pmIDStr(pmidlist[0]), pmErrStr(sts));
        exit(1);
    }
    if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
        fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }

    /* the thing we want is known to be the first value */
    pmExtractValue(rp->vset[0]->valfmt, rp->vset[0]->vlist, desclist[0].type,
            &atom, PM_TYPE_U32);
    pmFreeResult(rp);

    return atom.ul;
}

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
    float		u;
    pmAtomValue		atom;
    double		dt;


    /* get numpmid */
    numpmid_g = sizeof(pmclient_init) / sizeof(char *);
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
    if ((sts = pmLookupName(numpmid_g, pmclient_init, pmidlist_g)) < 0) {
        printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
        for (i = 0; i < numpmid_g; i++) {
            if (pmidlist_g[i] == PM_ID_NULL)
                fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), pmclient_init[i]);
        }
        exit(1);
    }
    /* pmLookupDesc: fill desclist */
    for (i = 0; i < numpmid_g; i++) {
        if ((sts = pmLookupDesc(pmidlist_g[i], &desclist_g[i])) < 0) {
            fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                    pmGetProgname(), pmclient_init[i], pmIDStr(pmidlist_g[i]), pmErrStr(sts));
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
    if (srp->vset[RELEASE]->numval <= 0 || srp->vset[VERSION]->numval <= 0 ||
            srp->vset[SYSNAME]->numval <= 0 || srp->vset[MACHINE]->numval <= 0 ||
            srp->vset[DISTRO]->numval <= 0) {

    }
    else {
        /* load average ... process all values, matching up the instances */
        pmExtractValue(srp->vset[RELEASE]->valfmt,
                &srp->vset[RELEASE]->vlist[i],
                desclist_g[RELEASE].type, &atom, PM_TYPE_STRING);
        release = (char *) malloc(strlen(atom.cp));
        strcpy(release, atom.cp);
        free(atom.cp);

        pmExtractValue(srp->vset[VERSION]->valfmt,
                &srp->vset[VERSION]->vlist[i],
                desclist_g[VERSION].type, &atom, PM_TYPE_STRING);
        version = (char *)malloc(strlen(atom.cp));
        strcpy(version, atom.cp);
        free(atom.cp);

        /* FIXME *********************************************************
         * Uncomment these lines to get Segmentation fault (core dumped)
         * why? I don't know,,, I'll debug It later...
         *****************************************************************/

        ///pmExtractValue(srp->vset[SYSNAME]->valfmt,
                ///&srp->vset[SYSNAME]->vlist[i],
                ///desclist_g[SYSNAME].type, &atom, PM_TYPE_STRING);
        ///sysname = (char *)malloc(strlen(atom.cp));
        ///strcpy(sysname, atom.cp);
        ///free(atom.cp);
///
        ///pmExtractValue(srp->vset[MACHINE]->valfmt,
                ///&srp->vset[MACHINE]->vlist[i],
                ///desclist_g[MACHINE].type, &atom, PM_TYPE_STRING);
        ///machine = (char *)malloc(strlen(atom.cp));
        ///strcpy(machine, atom.cp);
        ///free(atom.cp);
///
        ///pmExtractValue(srp->vset[DISTRO]->valfmt,
                ///&srp->vset[DISTRO]->vlist[i],
                ///desclist_g[DISTRO].type, &atom, PM_TYPE_STRING);
        ///distro = (char *)malloc(strlen(atom.cp));
        ///strcpy(distro, atom.cp);
        ///free(atom.cp);

    }
    /* free very old result */
    pmFreeResult(srp);
}

    static void
get_sample(info_t *ip)
{
    static pmResult	*crp = NULL;	/* current */
    static pmResult	*prp = NULL;	/* prior */
    static int		first = 1;
    static int		numpmid;
    static pmID		*pmidlist;
    static pmDesc	*desclist;
    static int		inst1;
    static int		inst15;
    static pmUnits	mbyte_scale;
    static int      instaid[numfetchs];

    int			sts;
    int			i;
    float		u;
    pmAtomValue		tmp;
    pmAtomValue		atom;
    double		dt;

    if (first) {
        /* first time initialization */
        mbyte_scale.dimSpace = 1;
        mbyte_scale.scaleSpace = PM_SPACE_MBYTE;

        /* get numpmid */
        numpmid = sizeof(pmclient_sample) / sizeof(char *);
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
        if ((sts = pmLookupName(numpmid, pmclient_sample, pmidlist)) < 0) {
            printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
            for (i = 0; i < numpmid; i++) {
                if (pmidlist[i] == PM_ID_NULL)
                    fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), pmclient_sample[i]);
            }
            exit(1);
        }
        /* pmLookupDesc: fill desclist */
        for (i = 0; i < numpmid; i++) {
            if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
                fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                        pmGetProgname(), pmclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
                exit(1);
            }
        }
    } // end of: if(first)

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &crp)) < 0) {
        fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
        exit(1);
    }


    if (first) {
        /*
         * from now on, just want the 1 minute and 15 minute load averages,
         * so limit the instance profile for this metric
         */
        if (opts.context == PM_CONTEXT_ARCHIVE) {
            inst1 = pmLookupInDomArchive(desclist[LOADAV].indom, "1 minute");
            inst15 = pmLookupInDomArchive(desclist[LOADAV].indom, "15 minute");
        }
        else {
            inst1 = pmLookupInDom(desclist[LOADAV].indom, "1 minute");
            inst15 = pmLookupInDom(desclist[LOADAV].indom, "15 minute");
        }
        if (inst1 < 0) {
            fprintf(stderr, "%s: cannot translate instance for 1 minute load average\n", pmGetProgname());
            exit(1);
        }
        if (inst15 < 0) {
            fprintf(stderr, "%s: cannot translate instance for 15 minute load average\n", pmGetProgname());
            exit(1);
        }
        pmDelProfile(desclist[LOADAV].indom, 0, NULL);	/* all off */
        pmAddProfile(desclist[LOADAV].indom, 1, &inst1);
        pmAddProfile(desclist[LOADAV].indom, 1, &inst15);

        first = 0;
    }


    /* if the second or later sample, pick the results apart */
    if (prp !=  NULL) {

        dt = pmtimevalSub(&crp->timestamp, &prp->timestamp);

        /*
         * But first ... is all the data present?
         */
        if (prp->vset[CPU_USR]->numval <= 0 || crp->vset[CPU_USR]->numval <= 0 ||
                prp->vset[CPU_SYS]->numval <= 0 || crp->vset[CPU_SYS]->numval <= 0) {
            ip->cpu_util = -1;
            ip->peak_cpu = -1;
            ip->peak_cpu_util = -1;
        }
        else {
            ip->cpu_util = 0;
            ip->peak_cpu_util = -1;	/* force re-assignment at first CPU */
            for (i = 0; i < ncpu; i++) {
                pmExtractValue(crp->vset[CPU_USR]->valfmt,
                        &crp->vset[CPU_USR]->vlist[i],
                        desclist[CPU_USR].type, &atom, PM_TYPE_FLOAT);
                u = atom.f;
                pmExtractValue(prp->vset[CPU_USR]->valfmt,
                        &prp->vset[CPU_USR]->vlist[i],
                        desclist[CPU_USR].type, &atom, PM_TYPE_FLOAT);
                u -= atom.f;
                pmExtractValue(crp->vset[CPU_SYS]->valfmt,
                        &crp->vset[CPU_SYS]->vlist[i],
                        desclist[CPU_SYS].type, &atom, PM_TYPE_FLOAT);
                u += atom.f;
                pmExtractValue(prp->vset[CPU_SYS]->valfmt,
                        &prp->vset[CPU_SYS]->vlist[i],
                        desclist[CPU_SYS].type, &atom, PM_TYPE_FLOAT);
                u -= atom.f;
                /*
                 * really should use pmConvertValue, but I _know_ the times
                 * are in msec!
                 */
                u = u / (1000 * dt);

                if (u > 1.0)
                    /* small errors are possible, so clip the utilization at 1.0 */
                    u = 1.0;
                ip->cpu_util += u;
                if (u > ip->peak_cpu_util) {
                    ip->peak_cpu_util = u;
                    ip->peak_cpu = i;
                }
            }
            ip->cpu_util /= ncpu;
        }

        /* freemem - expect just one value */
        if (prp->vset[FREEMEM]->numval <= 0 || crp->vset[FREEMEM]->numval <= 0) {
            ip->freemem = -1;
        }
        else {
            pmExtractValue(crp->vset[FREEMEM]->valfmt, crp->vset[FREEMEM]->vlist,
                    desclist[FREEMEM].type, &tmp, PM_TYPE_FLOAT);
            /* convert from today's units at the collection site to Mbytes */
            sts = pmConvScale(PM_TYPE_FLOAT, &tmp, &desclist[FREEMEM].units,
                    &atom, &mbyte_scale);
            if (sts < 0) {
                /* should never happen */
                if (pmDebugOptions.value) {
                    fprintf(stderr, "%s: get_sample: Botch: %s (%s) scale conversion from %s", 
                            pmGetProgname(), pmIDStr(desclist[FREEMEM].pmid), pmclient_sample[FREEMEM], pmUnitsStr(&desclist[FREEMEM].units));
                    fprintf(stderr, " to %s failed: %s\n", pmUnitsStr(&mbyte_scale), pmErrStr(sts));
                }
                ip->freemem = 0;
            }
            else
                ip->freemem = atom.f;
        }

        /* disk IOPS - expect just one value, but need delta */
        if (prp->vset[DKIOPS]->numval <= 0 || crp->vset[DKIOPS]->numval <= 0) {
            ip->dkiops = -1;
        }
        else {
            pmExtractValue(crp->vset[DKIOPS]->valfmt, crp->vset[DKIOPS]->vlist,
                    desclist[DKIOPS].type, &atom, PM_TYPE_U32);
            ip->dkiops = atom.ul;
            pmExtractValue(prp->vset[DKIOPS]->valfmt, prp->vset[DKIOPS]->vlist,
                    desclist[DKIOPS].type, &atom, PM_TYPE_U32);
            ip->dkiops -= atom.ul;
            ip->dkiops = ((float)(ip->dkiops) + 0.5) / dt;
        }

        /* load average ... process all values, matching up the instances */
        ip->load1 = ip->load15 = -1;
        for (i = 0; i < crp->vset[LOADAV]->numval; i++) {
            pmExtractValue(crp->vset[LOADAV]->valfmt,
                    &crp->vset[LOADAV]->vlist[i],
                    desclist[LOADAV].type, &atom, PM_TYPE_FLOAT);
            if (crp->vset[LOADAV]->vlist[i].inst == inst1)
                ip->load1 = atom.f;
            else if (crp->vset[LOADAV]->vlist[i].inst == inst15)
                ip->load15 = atom.f;
        }

        // FIXME: sort algorithm not work as I expected
        for(i = 0; i < crp->numpmid; i++) {
            if( crp->vset[i]->numval > 1)
                qsort(crp->vset[MEMRSS]->vlist, crp->vset[i]->numval, sizeof(pmValue), cmpfunc);
        }

        // FIXME: i want numval here dynamic - get num rows from ncurses ,, 
        //for(i = 0; i < crp->vset[MEMRSS]->numval; i++) {
        for(i = 0; i < numfetchs; i++) { 
            pmExtractValue(crp->vset[MEMRSS]->valfmt, &crp->vset[MEMRSS]->vlist[i], desclist[MEMRSS].type, &atom, PM_TYPE_U64);
            //printf("valfmt: %d\n", crp->vset[MEMRSS]->valfmt); // valfmt == 3
            //printf("deslist[MEMRSS].type: %d \n", desclist[MEMRSS].type); // pmDesc.type == 3
            //printf("inst: %d value: %" PRIu64 "\n", crp->vset[MEMRSS]->vlist[i].inst, atom.ull);

            // I couldn't print the value without pmExtractVlaue ,, crp->vset[MEMRSS]->vlist[i]->value.pval->vbuf[0] ?? wrong!
            ip->memrss[i]   = atom.ul; 
            ip->instaid[i]     = desclist[MEMRSS].indom;
        }

        /* free very old result */
        pmFreeResult(prp);
    } // end of: if(second or later)
    ip->timestamp = crp->timestamp;

    /* swizzle result pointers */
    prp = crp;
    }



int
main(int argc, char **argv)
{
    int			c, i;
    int			sts;
    int			samples;
    int			pauseFlag = 0;
    int			lines = 0;
    char		*source;
    const char		*host;
    info_t		info;
    //general_t		general;
    char		timebuf[26];	/* for pmCtime result */
    int         *b[numfetchs];
    char        *buf[numfetchs];
    char        **buff[numfetchs];

    setlinebuf(stdout);
    // ==========================================================
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

    host = pmGetContextHostName(c);
    ncpu = get_ncpu();

    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
        opts.interval.tv_sec = 5;

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

    /* claculate num of rows and fill the window according to this number */
    ///for(i = 0; i < new_y - score_size; i++) {
    ///int xxx = pmGetInDom(info.instaid[i], &b[i], &buff[i]);
    ///}



    while(samples == -1 || samples-- > 0) {
        getmaxyx(stdscr, new_y, new_x);
        /* - report format
           CPU  Busy    Busy  Free Mem   Disk     Load Average
           Util   CPU    Util  (Mbytes)   IOPS    1 Min  15 Min
           X.XXX   XXX   X.XXX XXXXX.XXX XXXXXX  XXXX.XX XXXX.XX
           */



        //if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
        //   __pmtimevalSleep(opts.interval);
        //get_sample(&info);

        get_sample(&info);

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


        for(i = 0; i < new_y - score_size; i++) {
            mvwprintw(new, i, 1, "%s ", "indom: "); // FIXME: I will merge this line and next line
            wprintw(new, "%d ", info.memrss[i]);
            ///wprintw(new, "ID = [%d]  ", info.instaid[i]);
            ///wprintw(new, "Internal = %d XXX = %s", *b[i], "label");
        }


        mvwprintw(new, i, 1, "%s ", "indom: "); // FIXME: I will merge this line and next line




        mvwprintw(logo, 1, 2, "RELEASE: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", release);
        wattroff(logo, COLOR_PAIR(1));


        wprintw(logo, " VERSION: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", version);
        wattroff(logo, COLOR_PAIR(1));


        wprintw(logo, " SYSNAME: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", sysname);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " MACHINE: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", machine);
        wattroff(logo, COLOR_PAIR(1));

        wprintw(logo, " DISTRO: ");
        wattron(logo, COLOR_PAIR(1));
        wprintw(logo, "%s ", distro);
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

/*
* FIXME: logical problem, I read the impl of pmSortInstances,
* it is too easy to sort by instances because integers for granted,
* If I'm not should of  the type of value,, 
*/
int
cmpfunc(const void *a, const void *b)
{
    pmValue *ap = (pmValue *)a;
    pmValue *bp = (pmValue *)b;

    //return ap->value.lval - ap->value.lval;
    //return ap->value.pval->*vbuf - bp->value.pval->*vbuf;
    //return ap->value.pval->&vbuf - bp->value.pval->&vbuf;
    return ap->value.pval->vbuf[0] - bp->value.pval->vbuf[0];
}
