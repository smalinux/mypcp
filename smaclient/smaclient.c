#include "ncurses.h"
#include "pmapi.h"
#include "libpcp.h"
#include "pmnsmap.h"


pmOptions opts = {
    .flags = PM_OPTFLAG_STDOUT_TZ | PM_OPTFLAG_BOUNDARIES,
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
} info_t;

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
	wprintw(stderr, "%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	wprintw(stderr, "%s: metric \"%s\" not in name space\n",
			pmGetProgname(), pmclient_init[0]);
	exit(1);
    }
    if ((sts = pmLookupDesc(pmidlist[0], desclist)) < 0) {
	wprintw(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		pmGetProgname(), pmclient_init[0], pmIDStr(pmidlist[0]), pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, pmidlist, &rp)) < 0) {
	wprintw(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* the thing we want is known to be the first value */
    pmExtractValue(rp->vset[0]->valfmt, rp->vset[0]->vlist, desclist[0].type,
		   &atom, PM_TYPE_U32);
    pmFreeResult(rp);

    return atom.ul;
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

	numpmid = sizeof(pmclient_sample) / sizeof(char *);
	if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
	    wprintw(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
	    exit(1);
	}
	if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
	    wprintw(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
	    exit(1);
	}
	if ((sts = pmLookupName(numpmid, pmclient_sample, pmidlist)) < 0) {
	    wprintw("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	    for (i = 0; i < numpmid; i++) {
		if (pmidlist[i] == PM_ID_NULL)
		    wprintw(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), pmclient_sample[i]);
	    }
	    exit(1);
	}
	for (i = 0; i < numpmid; i++) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
		wprintw(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
		    pmGetProgname(), pmclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	}
    }

    /* fetch the current metrics */
    if ((sts = pmFetch(numpmid, pmidlist, &crp)) < 0) {
	wprintw(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
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
	    wprintw(stderr, "%s: cannot translate instance for 1 minute load average\n", pmGetProgname());
	    exit(1);
	}
	if (inst15 < 0) {
	    wprintw(stderr, "%s: cannot translate instance for 15 minute load average\n", pmGetProgname());
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
		    wprintw(stderr, "%s: get_sample: Botch: %s (%s) scale conversion from %s", 
			pmGetProgname(), pmIDStr(desclist[FREEMEM].pmid), pmclient_sample[FREEMEM], pmUnitsStr(&desclist[FREEMEM].units));
		    wprintw(stderr, " to %s failed: %s\n", pmUnitsStr(&mbyte_scale), pmErrStr(sts));
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

	/* free very old result */
	pmFreeResult(prp);
    }
    ip->timestamp = crp->timestamp;

    /* swizzle result pointers */
    prp = crp;
}

int
main(int argc, char **argv)
{
    int			c;
    int			sts;
    int			samples;
    int			pauseFlag = 0;
    int			lines = 0;
    char		*source;
    const char		*host;
    info_t		info;		/* values to report each sample */
    char		timebuf[26];	/* for pmCtime result */
    WINDOW		*new;
    int 		height;
    int 		width;


    setlinebuf(stdout);
    initscr();
    getmaxyx(stdscr, height, width);
    new = newwin(height -2, width -2, 1, 1);
    scrollok(new, TRUE);

    /* SMA: START Handle params */
    /* SMA: END Handle params */
    opts.context = PM_CONTEXT_HOST;
    source = "local:";

    sts = c = pmNewContext(opts.context, source);

    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
	pmflush();
	exit(1);
    }

    host = pmGetContextHostName(c);
    wprintw(new, "HOST: %s\n",host);
    ncpu = get_ncpu();

    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
	opts.interval.tv_sec = 1;


    /* prime the results */
	get_sample(&info);	/* first pmFetch will prime the results */

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    while (samples == -1 || samples-- > 0) {
	if (lines % 15 == 0) {
	    time_t	time;
	    time = info.timestamp.tv_sec;
	    wprintw(new, "Host: %s, %d cpu(s), %s",
		    host, ncpu,
		    pmCtime(&time, timebuf));
/* - report format
  CPU  Busy    Busy  Free Mem   Disk     Load Average
 Util   CPU    Util  (Mbytes)   IOPS    1 Min  15 Min
X.XXX   XXX   X.XXX XXXXX.XXX XXXXXX  XXXX.XX XXXX.XX
*/

	    wprintw(new, "  CPU");
	    if (ncpu > 1)
		wprintw(new, "  Busy    Busy");
	    wprintw(new, "  Free Mem   Disk     Load Average\n");
	    wprintw(new, " Util");
	    if (ncpu > 1)
		wprintw(new, "   CPU    Util");
	    wprintw(new, "  (Mbytes)   IOPS    1 Min  15 Min\n");
	}
	if (opts.context != PM_CONTEXT_ARCHIVE || pauseFlag)
	    __pmtimevalSleep(opts.interval);
	get_sample(&info);


	if (info.cpu_util >= 0)
	    wprintw(new, "%5.2f", info.cpu_util);
	else
	    wprintw(new, "%5.5s", "?");
	if (ncpu > 1) {
	    if (info.peak_cpu >= 0)
		wprintw(new, "   %3d", info.peak_cpu);
	    else
		wprintw(new, "   %3.3s", "?");
	    if (info.peak_cpu_util >= 0)
		wprintw(new, "   %5.2f", info.peak_cpu_util);
	    else
		wprintw(new, "   %5.5s", "?");
	}
	if (info.freemem >= 0)
	    wprintw(new, " %9.3f", info.freemem);
	else
	    wprintw(new, " %9.9s", "?");
	if (info.dkiops >= 0)
	    wprintw(new, " %6d", info.dkiops);
	else
	    wprintw(new, " %6.6s", "?");
	if (info.load1 >= 0)
	    wprintw(new, "  %7.2f", info.load1);
	else
	    wprintw(new, "  %7.7s", "?");
	if (info.load15 >= 0)
	    wprintw(new, "  %7.2f\n", info.load15);
	else
	    wprintw(new, "  %7.7s\n", "?");
 	lines++;
	wrefresh(new);
    }
    endwin();
    exit(0);
}
