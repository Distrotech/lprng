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
# Makefile,v 3.3 1998/03/30 02:33:23 papowell Exp
########################################################################## 

.PHONY: all clean ci install realclean mostlyclean distclean

all clean install realclean mostlyclean distclean:
	echo done

CI=
CO=-kv

cifast ci:
	checkin() { \
		ci $(CI) -mUpdate -t-Initial $$1; \
		co $(CO) $$1; \
	}; \
	for i in *; do \
		echo $$i ; \
		if [ -f "$$i" ] ; then \
			checkin $$i; \
		fi; \
	done;
