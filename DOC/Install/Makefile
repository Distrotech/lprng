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
# $Id: Makefile,v 3.0 1996/05/19 04:06:54 papowell Exp $
########################################################################## 

.PHONY: all clean ci install FRC realclean mostlyclean distclean

INSTALL_DIR= ..
DOC= Install.txt Install.ps

all: $(DOC)

clean realclean mostlyclean distclean:
	-rm -f $(DOC)

install: $(DOC)
	mv $(DOC) $(INSTALL_DIR)

Install.txt: Install.nroff
	typeset -O$@ $^
Install.ps: Install.nroff
	typeset -rt1 -Tps -O$@ $^

ci:
	for i in * ; do \
		if [ -f $$i ] ; then ci $(CI) -l -mUpdate -t-Initial $$i; fi; \
	done;
