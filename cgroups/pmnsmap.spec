pmclient_init {
    hinv.ncpu			NUMCPU
}

pmclient_sample {
    kernel.all.load	LOADAV
    kernel.percpu.cpu.user	CPU_USR
    kernel.percpu.cpu.sys	CPU_SYS
    mem.freemem		FREEMEM
    disk.all.total		DKIOPS

    kernel.uname.release RELEADE
    cgroup.pressure.memory.full.total CGP_total
}

cgroups {
}
