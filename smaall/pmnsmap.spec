pmclient_init {
    hinv.ncpu			NUMCPU
}

pmclient_sample {
    kernel.all.load	LOADAV
    kernel.percpu.cpu.user	CPU_USR
    kernel.percpu.cpu.sys	CPU_SYS
    mem.freemem		FREEMEM
    disk.all.total		DKIOPS
    kernel.uname.release RELEASE
    kernel.uname.version VERSION
    kernel.uname.sysname SYSNAME
    kernel.uname.machine MACHINE
    kernel.uname.nodename NODENAME
    kernel.uname.distro DISTRO
}

sohaib_cpu {
	hinv.cpu.vendor 	SMACPU
}
