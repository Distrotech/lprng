#!/bin/sh
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: TESTSUPPORT/filter.sh
# PURPOSE: test filter for LPR software
# filter.sh,v 3.1 1996/12/28 21:40:46 papowell Exp
########################################################################## 
#	Filter Dummy Test
# 
PATH=/bin:/usr/bin
echo FILTER $$ $0 $* 1>&2
echo FILTER $$ "pwd " `/bin/pwd`  1>&2
delay=0
for i in $*
do
	case $i in
		-delay*) delay=`echo $i |sed -e 's/-delay//'` ;;
		-error*) error=`echo $i |sed -e 's/-error//'` ;;
		-*) ;;
		*) file=$i ;;
	esac
done
#if test -n "$file";
#	then echo $0 $* >>$file
#	else echo "--- NO Accounting File ---" 1>&2
#fi
# echo information into output
echo $0 $*
# wait a minute to simulate the delay
echo FILTER $$ delay $delay 1>&2
if test "$delay" -ne 0 ; then
	echo FILTER $$ sleeping $delay 1>&2
	sleep $delay;
	echo FILTER $$ awake 1>&2
fi;
echo FILTER processing 1>&2
# exit with error status
if test -n "$error";
then
	exit $error;
fi;
# pump stdin to stdout
cat
echo FILTER DONE 1>&2
exit 0;
