#
# -- START --
# $Id: preremove.solaris.sh,v 1.2 2000/06/06 18:16:09 papowell Exp papowell $
#
# This is the shell script that does the preremove
echo RUNNING preremove.solaris.sh
echo "Stopping LPD"
kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
exit 0
