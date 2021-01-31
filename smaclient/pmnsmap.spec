smaclient_init {
    kernel.uname.release        RELEASE
    kernel.uname.version        VERSION
    kernel.uname.sysname        SYSNAME
    kernel.uname.machine        MACHINE
    kernel.uname.distro         DISTRO
    hinv.ncpu			NUMCPU
}

smaclient_sample {
    proc.psinfo.rss     MEMRSS
}
