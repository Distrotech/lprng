#
# -- START --
# $Id: init.freebsd.sh,v 1.3 2000/06/05 18:30:23 papowell Exp papowell $
# This file can be installed in /usr/local/etc/init.d
#  as lprng.sh
# Freebsd 3.x and 4.x will run all files in this directory
#  with the suffix .sh as shell scripts
#
# If you do NOT replace the FreeBSD lpd with LRPng's
# in /usr/sbin/lpd,  then you should edit the /etc/rc.conf
# and set
#   lpd_enable=NO
#

case "$1" in
    restart ) 
			$0 stop
			sleep 1
			$0 start
            ;;
    stop  )
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
            ;;
    start )
            echo -n ' printer';
            ${LPD_PATH}
            ;;
esac
