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
# $Id: Makefile,v 1.1 1996/03/04 00:59:29 papowell Exp $
########################################################################## 

.PHONY: all clean ci install FRC realclean mostlyclean distclean

all realclean mostlyclean distclean clean install:

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
