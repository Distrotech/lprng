#
# -- START --
# init.generic.sh,v 1.1 2001/08/21 20:33:15 root Exp
# This file can be installed in /usr/local/etc/init.d or
# /etc/init.d as appropriate as lprng.sh or lprng.init or
# lpd.init depending on your system.
#

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
            echo -n ' printer';
            ${LPD_PATH}
            ;;
esac
