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
# $Id: Makefile,v 3.3 1997/01/19 14:34:56 papowell Exp $
########################################################################## 


all: Tutorial.txt Tutorial.ps
#	$(MAKE) MAKETARGET=$@ $(ALLDIRS)
.PHONY: all clean ci install FRC

clean:
	echo done


ci: cifiles
#	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

cifiles:
	-ci $(CI) -l -mUpdate -t-Initial $(CI) Makefile Using_stty Tutorial.nroff

Tutorial.txt: Tutorial.*.nrf
	groff -ms -Tascii >$@ Tutorial.*.nrf
	#typeset -ms -O$@ Tutorial.*.nrf

Tutorial.ps: Tutorial.*.nrf
	groff -ms -Tps >$@ Tutorial.*.nrf

install: Tutorial.ps Tutorial.txt
	mv $^ ..
