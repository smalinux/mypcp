/*
 * Performance Metrics Name Space Map
 * Built by pmgenmap.sh from the file
 * pmnsmap.spec
 * on Mon 01 Feb 2021 11:28:34 PM EET
 *
 * Do not edit this file!
 */

const char *smaclient_init[] = {
#define RELEASE	0
	"kernel.uname.release",
#define VERSION	1
	"kernel.uname.version",
#define SYSNAME	2
	"kernel.uname.sysname",
#define MACHINE	3
	"kernel.uname.machine",
#define DISTRO	4
	"kernel.uname.distro",
#define NUMCPU	5
	"hinv.ncpu",

};


const char *smaclient_sample[] = {
#define MEMRSS	0
	"proc.psinfo.rss",

};

