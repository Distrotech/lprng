###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: UTILS/Makefile
# PURPOSE: generate utilities
# $Id: Makefile,v 3.0 1996/05/19 04:06:59 papowell Exp $
########################################################################## 

.PHONY: all clean ci install FRC

all: Tutorial.txt Tutorial.ps
#	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

clean:
	echo done


#ALLDIRS = LISA-95

$(ALLDIRS): FRC
	$(MAKE) -C $@ $(MAKETARGET)

ci: cifiles
#	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

cifiles:
	-ci $(CI) -l -mUpdate -t-Initial $(CI) Makefile Using_stty Tutorial.nroff

Tutorial.txt: Tutorial.*.nrf
	typeset -ms -O$@ $^

Tutorial.ps: Tutorial.*.nrf
	typeset -ms -Tps -O$@ $^

install: Tutorial.ps Tutorial.txt
	mv $^ ..
