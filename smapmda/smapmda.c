/*
                                .--------------------PMCD
                               '
                          .---------.
                          |  SMA    |
          .---------------|  PMDA   |------------.
          |               '---------'            |
          |                    |                 |
          |                    |                 |
          v                    v                 v
       .-----.            .--------.        .---------.
       | mem |            | kernel |        | Counter |
       '-----'            '--------'        '---------'
      mem.total                             counter.c1
      mem.free                              counter.c2
                                            counter.c3


 * SMAPMDA built on top of Trivial PMDA
 *
 * Metrics
 *	trivial.mem.total   - read data from the first line of /proc/meminfo
 *	trivial.mem.free    - read data from the second line of /proc/meminfo
 *	trivial.counter		- dummy trivial counters
 *	trivial.kernel      - collect num from /sys/kernel/debug/sma_debugfs/sma_u8
 *	                        (my debugfs kernel module.)
 *
 * What's next?
 *  0. Read man pages:
 *      pmdaMain, pmdaFetch, pmdaDSO, pmdaDaemon, pmdaInit, 
 *      dbpmda, pmdbg, pmdaConnect, pmdaGetOptions,
 *      pmdaOpenLog, pmdacache, pmnotifyerr, 
 *  1. trace PCP/PMDAS source code & see more example.
 *  2. Change domain ID from TRIVIAL to ur ID, start from domain.h
 *  3. Solve all "FIXME".
 *  4. Support the rest of pmdaInteface function pointers.
 *  5. How I could print to stdout of debugging?
 *  6. Read & Study more glibc and sysprogramming header files..
 *  7. look at impl of:
 *      proc.fd.count - proc.nprocs - 
 *
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "domain.h"

/*
 * list of instances
 */

static pmdaInstid counter[] = {
    { 0, "c1" }, { 1, "c2" }, { 2, "c3" }
};

/*
 * instance domains
 */

static pmdaIndom indomtab[] = {
#define COUNTER_INDOM	0	/* serial number for "counter" instance domain */
    { COUNTER_INDOM, sizeof(counter)/sizeof(counter[0]), counter },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
    /* mem.total */
    { NULL, 
        { PMDA_PMID(1,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    /* mem.free */
    { NULL, 
        { PMDA_PMID(1,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    /* counter */
    { NULL, 
        { PMDA_PMID(0,1), PM_TYPE_U32, COUNTER_INDOM, PM_SEM_INSTANT, 
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
    /* kernel */
    { NULL, 
        { PMDA_PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
            PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
};

static char	*username;
static char	mypath[MAXPATHLEN];
static int	isDSO = 1;		/* ==0 if I am a daemon */
static int  c1 = 1, c2 = 100, c3 = 500;

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * wrapper for pmdaFetch which increments the fetch count and checks for
 * a change to the NOW instance domain.
 */
static int
smapmda_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    /* FIXME: Check if files changed & update */
    //smapmda_config_file_check();
    //smapmda_refresh_mem_checks();

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
smapmda_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    FILE            *f; 
    FILE            *k = fopen("/sys/kernel/debug/sma_debugfs/sma_u8", "r");
    int             unused;
    static int             kk = 4;
    char            comm[50];
    char            *p, name[1024];
    static char     *totalmem;

    /* FIXME: Add validation here. */

    if (cluster == 0 && item == 1) {		/* smapmda.counter */
        switch (inst) {
            case 0:				/* c1 */
                if (c1 % 1000 == 0)
                    c1 = 0;
                atom->ul = c1++;
                break;
            case 1:				/* c2 */
                if (c2 % 1000 == 0)
                    c2 = 100;
                atom->ul = c2++;
                break;
            case 2:				/* c3 */
                if (c3 % 1000 == 0)
                    c3 = 500;
                atom->ul = c3--;
                break;
            default:
                return PM_ERR_INST;
        }
    } else if (cluster == 1 && item == 3) {     /* smapmda.mem.total */
        f   = fopen("/proc/meminfo", "r");
        if(f != NULL) {
            while (fgets(name, sizeof(name), f)) {
                if (strncmp(name, "MemTotal", 8) == 0) {
                    if ((p = strstr(name, " kB")) != NULL)
                        totalmem = strndup(p+1, 8);
                    break;
                }
            }
            atom->cp = totalmem;
            fclose(f);
        }
        //
        ///fscanf(f, "%s %d", comm, &unused);
        ///atom->ul = unused;
    } else if (cluster == 1 && item == 4) {     /* smapmda.mem.free */
        /* Bad algoritm,, */
        atom->ul = 555; // FIXME: placeholder,,
    } else if (cluster == 2 && item == 2) {     /* smapmda.kernel   */
        //fscanf(k, "%d", &kk); HERE is a probelm, i think related to a file permission
        atom->ul = 444; /* FIXME: placeholder */
    } else {
        return PM_ERR_PMID;
    }

    return PMDA_FETCH_STATIC;
}

/*
 * wrapper for pmdaInstance which we need to ensure is called with the
 * _current_ contents of the NOW instance domain.
 */
static int
smapmda_instance(pmInDom indom, int foo, char *bar, pmInResult **iresp, pmdaExt *pmda)
{
    return pmdaInstance(indom, foo, bar, iresp, pmda);
}

/*
 * support the storage of a value into the number of fetches count
 * FIXME: Test you code: pmval -a ...
 * FIXME: when I add my metric to /var/lib/pcp/config/pmlogger/config.default
 * pmlogger override my setup, READ: pmlogger & pmlogconf(1),,
 */
static int
smapmda_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		j;
    int		val;
    int		sts = 0;
    pmValueSet	*vsp = NULL;

    /* a store request may affect multiple metrics at once */
    for (i = 0; i < result->numpmid; i++) {
        unsigned int	cluster;
        unsigned int	item;

        vsp = result->vset[i];
        cluster = pmID_cluster(vsp->pmid);
        item = pmID_item(vsp->pmid);

        if (cluster == 0) {	/* all storable metrics are cluster 0 */

            switch (item) {
                case 1:					/* smapmda.counter */
                    /* a store request may affect multiple instances at once */
                    for (j = 0; j < vsp->numval && sts == 0; j++) {

                        val = vsp->vlist[j].value.lval;
                        if (val < 0) {
                            sts = PM_ERR_BADSTORE;
                            val = 0;
                        }

                        switch (vsp->vlist[j].inst) {
                            case 0:				/* red */
                                c1 = val;
                                break;
                            case 1:				/* green */
                                c2 = val;
                                break;
                            case 2:				/* blue */
                                c3 = val;
                                break;
                            default:
                                sts = PM_ERR_INST;
                        }
                    }
                    break;

                default:
                    sts = PM_ERR_PMID;
                    break;
            }
        }
        else if ((cluster == 1 && 
                    (item == 2 || item == 3)) ||
                (cluster == 2 && item == 4)) {
            sts = PM_ERR_PERMISSION;
            break;
        }
        else {
            sts = PM_ERR_PMID;
            break;
        }
    }
    return sts;
}

static int
simple_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    int		serial;

    switch (type) {
        case PM_LABEL_DOMAIN:
            pmdaAddLabels(lpp, "{\"role\":\"testing\"}");
            break;
        case PM_LABEL_INDOM:
            serial = pmInDom_serial((pmInDom)ident);
            if (serial == COUNTER_INDOM) {
                pmdaAddLabels(lpp, "{\"indom_name\":\"color\"}");
                pmdaAddLabels(lpp, "{\"model\":\"RGB\"}");
            }
            break;
        case PM_LABEL_CLUSTER:
        case PM_LABEL_ITEM:
            /* no labels to add for these types, fall through */
        default:
            break;
    }
    return pmdaLabel(ident, type, lpp, pmda);
}

static int
simple_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    return 0;
}


/*
 * Initialise the agent (both daemon and DSO).
 */
void 
smapmda_init(pmdaInterface *dp)
{
    if (isDSO) {
        int sep = pmPathSeparator();
        pmsprintf(mypath, sizeof(mypath), "%s%c" "smapmda" "%c" "help",
                pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_2, "trivial DSO", mypath);
    } else {
        pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
        return;

    ///dp->version.any.instance = smapmda_fetch;
    ///dp->version.any.store = smapmda_store;
    ///dp->version.any.instance = smapmda_instance;
    ///dp->version.seven.label = simple_label; // FIXME

    pmdaSetFetchCallBack(dp, smapmda_fetchCallBack);
    pmdaSetLabelCallBack(dp, simple_labelCallBack); // FIXME

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
            sizeof(metrictab)/sizeof(metrictab[0]));
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = pmPathSeparator();
    pmdaInterface	desc;

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "smapmda" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmGetProgname(), TRIVIAL,
            "smapmda.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }
    if (opts.username)
        username = opts.username;

    pmdaOpenLog(&desc);
    smapmda_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
}
