#
# -- START --
# $Id: preremove.generic.sh,v 1.4 2000/06/06 18:16:08 papowell Exp papowell $
#
# This is the shell script that does the preremove
# lpd shutdown.  It is the script from hell
echo RUNNING preremove.generic.sh
echo "Stopping LPD"
kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
exit 0
