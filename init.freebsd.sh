#
# -- START --
# This file can be installed in /usr/local/etc/init.d
#  as lprng.sh
# Freebsd 3.x and 4.x will run all files in this directory
#  with the suffix .sh as shell scripts
#
# If you replace the FreeBSD lpd with LRPng's
# in /usr/sbin/lpd edit the /etc/rc.conf
# and set
#   lpd_enable="NO"
#   lprng_enable="YES"
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
		pid=
		if [ -f "$LOCKFILE" ] ; then
			pid=`cat $LOCKFILE`;
		fi
		if [ "$pid" != '' ] ; then
			kill -INT $pid >/dev/null 2>&1
		fi
		;;
    start )
		case "$lprng_enable" in
			[Nn][Oo] ) ;;
			* )
            echo -n ' printer';
            ${LPD_PATH}
            ;;
		esac
		;;
esac
