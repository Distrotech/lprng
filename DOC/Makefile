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
# $Id: Makefile,v 3.0 1996/05/19 04:06:53 papowell Exp $
########################################################################## 

.PHONY: all clean ci install FRC realclean mostlyclean distclean

all:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)
realclean mostlyclean distclean clean:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)
install:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

ALLDIRS = Intro Install

$(ALLDIRS): FRC
	$(MAKE) -C $@ $(MAKETARGET)


Install.txt: Install.nroff
	typeset -O$@ $^
Install.ps: Install.nroff
	typeset -rt1 -Tps -O$@ $^

DIRS= Install Intro


ci:
	checkin() { \
		ci $(CI) -l -mUpdate -t-Initial $$1; \
	}; \
	for i in * ; do \
		if [ -f "$$i" ] ; then \
			case $$i in \
			*.txt | *.ps ) ;; \
			* ) checkin $$i ;; \
			esac; \
		fi; \
	done;
	checkin() { \
		ci $(CI) -l -mUpdate -t-Initial $$1; \
	}; \
	for i in */* ; do \
		if [ -f "$$i" ] ; then \
			case $$i in \
			*.txt | *.ps | *RCS* | *,v ) ;; \
			* ) checkin $$i ;; \
			esac; \
		fi; \
	done;
