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
# $Id: Makefile,v 3.4 1997/01/19 14:34:56 papowell Exp $
########################################################################## 


INSTALL_DIR= ..
DOC= Install.txt Install.ps

all: $(DOC)

.PHONY: all clean ci install FRC realclean mostlyclean distclean

clean realclean mostlyclean distclean:
	-rm -f $(DOC)

install: $(DOC)
	mv $(DOC) $(INSTALL_DIR)

SRC=Install.nroff

Install.txt: $(SRC)
	groff -Tascii >$@ $(SRC)
	#typeset -O$@ $(SRC)
Install.ps: $(SRC)
	groff -rt1 -Tps >$@ $(SRC)
	#typeset -rt1 -Tps -O$@ $(SRC)

ci:
	for i in * ; do \
		if [ -f $$i ] ; then ci $(CI) -l -mUpdate -t-Initial $$i; fi; \
	done;
