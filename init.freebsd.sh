#
# -- START --
# init.freebsd.sh,v 1.1 2001/08/21 20:33:15 root Exp
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


lprng_enable="YES"
if [ -f /usr/local/etc/rc.subr ] ; then
	. /usr/local/etc/rc.subr
	load_rc_config lprng
	name=lprng
	rcvar=`set_rcvar`
	lprng_enable=`eval echo \\$\$rcvar`;
elif [ -f /etc/rc.conf ] ; then
	. /etc/rc.conf
fi

# ignore INT signal
trap '' 2

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
		if [ "$lprng_enable" != NO ] ; then
            echo -n ' printer';
            ${LPD_PATH}
		fi
            ;;
esac
