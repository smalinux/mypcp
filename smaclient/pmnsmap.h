/*
 * Performance Metrics Name Space Map
 * Built by pmgenmap.sh from the file
 * pmnsmap.spec
 * on Sat 23 Jan 2021 05:27:07 PM EET
 *
 * Do not edit this file!
 */

const char *pmclient_init[] = {
#define NUMCPU	0
	"hinv.ncpu",

};


const char *pmclient_sample[] = {
#define LOADAV	0
	"kernel.all.load",
#define CPU_USR	1
	"kernel.percpu.cpu.user",
#define CPU_SYS	2
	"kernel.percpu.cpu.sys",
#define FREEMEM	3
	"mem.freemem",
#define DKIOPS	4
	"disk.all.total",

};
