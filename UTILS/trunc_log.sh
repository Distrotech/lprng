#!/bin/sh
#===========================================================================
#= File:                                                                   =
#=   trunc_log.sh                                                          =
#=                                                                         =
#= Synopsis:                                                               =
#=   This script is to be run by cron to reset the size of files
# Mon Sep  4 08:10:13 PDT 1995 Patrick Powell
#
# NB- first originated in V7 UNIX.

#FILES= /usr/adm/lpd-errs
FILES="/tmp/a"

for i in $FILES; do
	if [ -f "$i" ]; then
		tail -300 "$i" >/tmp/$$_rm
		cat /tmp/$$_rm >"$i"
		rm -f /tmp/$$_rm
	fi;
done
