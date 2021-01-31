pmclient_init {
    hinv.ncpu			NUMCPU
    kernel.uname.release        RELEASE
    kernel.uname.version        VERSION
    kernel.uname.sysname        SYSNAME
    kernel.uname.machine        MACHINE
    kernel.uname.distro         DISTRO

}

pmclient_sample {
    kernel.all.load	LOADAV
    kernel.percpu.cpu.user	CPU_USR
    kernel.percpu.cpu.sys	CPU_SYS
    mem.freemem		FREEMEM
    disk.all.total		DKIOPS
    proc.psinfo.rss     MEMRSS
}
