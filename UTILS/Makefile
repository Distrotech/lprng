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
# $Id: Makefile,v 3.0 1996/05/19 04:06:47 papowell Exp $
########################################################################## 

.PHONY: all clean ci install realclean mostlyclean distclean

all clean install realclean mostlyclean distclean:
	echo done

ci:
	checkin() { \
		ci $(CI) -l -mUpdate -t-Initial $$1; \
	}; \
	for i in *; do \
		echo $$i ; \
		if [ -f "$$i" ] ; then \
			checkin $$i; \
		fi; \
	done;
