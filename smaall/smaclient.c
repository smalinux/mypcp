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
    pmDesc	mydesc;
    pmResult	*rp;
    pmAtomValue	atom;
    int		sts;
    char	*buf;
    char	**buff;
    int 	num = NULL;
    char	tmp[200]; 

    sts = pmLookupName(1, pmclient_init, pmidlist);
    sts = pmLookupDesc(pmidlist, desclist);
    pmLookupText(pmidlist[0], PM_TEXT_ONELINE, &buf);
    printf("oneline desc: %s\n", buf);

    pmNameInDom(desclist[0].indom, 0, &buf);
    printf("&&&&&&&&&&&& pmNameInDom: %s\n", buf);

    /*pmLookupDesc(pmidlist, &mydesc);
      pmLookupInDomText(mydesc.indom, PM_TEXT_ONELINE, &buf);
      printf("oneline desc: %s\n", buf);
      */

    sts = pmFetch(1, pmidlist, &rp);

    // print pmID as $pminfo -m 
    printf("Name of pmID: %s \n", pmIDStr(rp->vset[0]->pmid));

    // print metric name: "hinv.cpu" - although not work here!
    pmNameID(pmidlist, &buf);
    printf("=Metric name of pmID: %s \n", buf);
    sts = pmNameAll(pmidlist[0], &buff);
    __pmPrintMetricNames(stdout, sts, buff, " or ");
    printf("\n");	// or pmflush();

    // pmGetChildren
    int i = 0;
    num = pmGetChildren("kernel.uname", &buff);
    printf("number: %d \n", num);
    __pmPrintMetricNames(stdout, num, buff, " or ");
    printf("\n");	// or pmflush();
    //free(buff);


    pmInDom 	idm;
    pmLookupInDom(idm, "/proc/pressure/memory");
    printf("pmLookupInDom: %s\n", idm);

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

    char		*buf;
    char		**buff;

    if (first) {
        /* first time initialization */
        mbyte_scale.dimSpace = 1;
        mbyte_scale.scaleSpace = PM_SPACE_MBYTE;

        numpmid = sizeof(pmclient_sample) / sizeof(char *);
        if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmidlist[0]))) == NULL) {
            fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
            exit(1);
        }
        if ((desclist = (pmDesc *)malloc(numpmid * sizeof(desclist[0]))) == NULL) {
            fprintf(stderr, "%s: get_sample: malloc: %s\n", pmGetProgname(), osstrerror());
            exit(1);
        }
        if ((sts = pmLookupName(numpmid, pmclient_sample, pmidlist)) < 0) {
            printf("%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
            for (i = 0; i < numpmid; i++) {
                if (pmidlist[i] == PM_ID_NULL)
                    fprintf(stderr, "%s: metric \"%s\" not in name space\n", pmGetProgname(), pmclient_sample[i]);
            }
            exit(1);
        }
        for (i = 0; i < numpmid; i++) {
            if ((sts = pmLookupDesc(pmidlist[i], &desclist[i])) < 0) {
                fprintf(stderr, "%s: cannot retrieve description for metric \"%s\" (PMID: %s)\nReason: %s\n",
                        pmGetProgname(), pmclient_sample[i], pmIDStr(pmidlist[i]), pmErrStr(sts));
                exit(1);
            }
            // SMA: pmNameID
            pmNameID(pmidlist[i], &buf);
            printf("pmNameID pmID: %s \n", buf);
            pmNameInDom(desclist[0].indom, 0, &buf);
            printf("&&&&&&&&&&&& pmNameInDom: %s\n", buf);

            // SMA: pmNameAll 
            sts = pmNameAll(pmidlist[i], &buff);
            __pmPrintMetricNames(stdout, sts, buff, " or ");
            printf("\n");
        }
    }

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
            printf(">>>>>>> %d\n", inst15);
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


    setlinebuf(stdout);

    /* SMA: START Handle params */
    // Blank.
    /* SMA: END Handle params */

    // SMA: Start Make new context
    // pmNewContext(PM_CONTEXT_HOST, "127.0.0.1");
    opts.context = PM_CONTEXT_LOCAL;
    sts = c = pmNewContext(opts.context, "");
    // SMA: End Make new context

    /* complete TZ and time window option (origin) setup */
    if (pmGetContextOptions(c, &opts)) {
        pmflush();
        exit(1);
    }


    /* set a default sampling interval if none has been requested */
    if (opts.interval.tv_sec == 0 && opts.interval.tv_usec == 0)
        opts.interval.tv_sec = 1;


    /* prime the results */
    get_sample(&info);	/* first pmFetch will prime the results */

    /* set sampling loop termination via the command line options */
    samples = opts.samples ? opts.samples : -1;

    // ========================================================================

    // Program name manipulation
    printf("Default program name: %s\n", pmGetProgname());
    pmSetProgname("smaall");
    printf("New program name: %s\n", pmGetProgname());

    /*
     * Start PMAPI Context
     */


    /*
     * End PMAPI Context
     */

    /*
     * Start timeval manipulation
     *$ man && 3.8.10 PCP guide.
     */ 
    time_t time; 
    // pmtimevalNow
    pmtimevalNow(&time);
    printf("pmtimevalNow: %s", pmCtime(&time, timebuf));

    // pmtimevalInc, pmtimevalDec, pmtimevalAdd, pmtimevalSub,
    // pmtimevalToReal, pmtimevalFromReal, pmPrintStamp, pmPrintHighResStamp
    pmtimevalInc(&time, &time);
    printf("pmtimevalInc: %s", pmCtime(&time, timebuf));

    // another way: Google.
    time = info.timestamp.tv_sec;
    printf("Time: %s", pmCtime(&time, timebuf));


    /*
     * End timeval manipulation
     */

    printf("Rright?!===============================\n");
    // hostname
    host = pmGetContextHostName(c);
    printf("pmGetContextHostNam (Host simply!): %s\n",host);

    // cpu
    ncpu = get_ncpu();
    printf("%d cpu(s)\n", ncpu);

    /*
     * Start Metric Description
     */

    /*
     * End Metric Description
     */

    /*
     * Start Timezone
     */


    /*
     * End Timezone
     */

    pmID	*pmid;
    pmDesc	*desc;
    pmDesc	mydesc;
    char	*buf;
    int 	err;
    int 	numm;
    int 	i;
    int 	ipmLIDm;
    char 	*str_ver;

    numm = sizeof(sohaib_cpu) / sizeof(char *);
    pmid = (pmID *)malloc(numm * sizeof(pmid[0]));
    desc = (pmDesc *)malloc(numm * sizeof(desc[0]));
    pmLookupName(numm, sohaib_cpu, pmid);
    for (i = 0; i < numm; i++) {
        pmLookupDesc(pmid[i], &desc[i]);
        err = pmNameInDom(desc[0].indom, 1, &buf);
        printf("9999999999999999 pmNameInDom: %s\n", buf);
        ipmLIDm = pmLookupInDom(desc[0].indom, "cpu3");
        printf("pmLookupInDom: %d\n", ipmLIDm);
    }


    /*
     * Start pmGetConfig
     */
    str_ver = pmGetConfig("PCP_VERSION");
    printf("pmGetConfig: %d\n", str_ver);

    /*
     * End pmGetConfig
     */

    char        *usr_name;
    pmGetUsername(&usr_name);
    printf("username: %s\n", usr_name);

    printf("PMAPI version: %d\n", PMAPI_VERSION);


    exit(0);
}
