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
# $Id: Makefile,v 3.2 1997/01/19 14:34:56 papowell Exp $
########################################################################## 


all:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

.PHONY: all clean ci install FRC realclean mostlyclean distclean

realclean mostlyclean distclean clean:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)
install:
	$(MAKE) MAKETARGET=$@ $(ALLDIRS)

ALLDIRS = Intro Install Tutorial Macros

$(ALLDIRS): FRC
	(cd $@; $(MAKE) $(MAKETARGET))
FRC:


Install.txt: Install.nroff
	typeset -O$@ $^
Install.ps: Install.nroff
	typeset -rt1 -Tps -O$@ $^

DIRS= Install Intro


ci:
	for i in $(ALLDIRS) ; do \
		if test ! -d $$i/RCS ; then \
			mkdir $$i/RCS ; \
		fi; \
	done;
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
