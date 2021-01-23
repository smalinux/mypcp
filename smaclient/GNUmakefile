#
# Copyright (c) 2013-2016 Red Hat.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 

TOPDIR = ../pcp
include $(TOPDIR)/src/include/builddefs

CFILES	= smaclient.c 
CMDTARGET = smaclient$(EXECSUFFIX)
TARGETS = $(CMDTARGET)

LLDLIBS	= $(PCPLIB)
LDIRT	= pmnsmap.h mylog.* runme.sh *.o $(TARGETS)
CONFIGS	= pmnsmap.spec pmlogger.config
DEMODIR	= $(PCP_DEMOS_DIR)/pmclient

default:	$(TARGETS)

include $(BUILDRULES)

install:	$(TARGETS)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install $(DEMODIR)/Makefile
	$(INSTALL) -m 644 $(CFILES) $(CONFIGS) README $(DEMODIR)
	$(INSTALL) -m 755 $(PYFILES) $(DEMODIR)/$(PYFILES)

smaclient.o:	pmnsmap.h

smaclient$(EXECSUFFIX):  smaclient.o 
	rm -f $@
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

pmgenmap.sh:
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	$(TOPDIR)/src/pmgenmap/pmgenmap.sh > pmgenmap.sh

pmnsmap.h:	pmgenmap.sh pmnsmap.spec
	$(PMGENMAP) pmnsmap.spec > pmnsmap.h

default_pcp:	default

install_pcp:	install

smaclient.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check::	$(CFILES)
	$(CLINT) $(CFILES)
	$(PYLINT) $(PYFILES)
