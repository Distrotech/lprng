#
# -- START --
# $Id: init.solaris.sh,v 1.4 2000/06/05 18:30:23 papowell Exp papowell $
#
# for Solaris
#  copy this script to /etc/init.d/lprng
#  Then make links as required in /etc/rc2.d to the script
#  cp init.solaris /etc/init.d/lprng
#  (cd /etc/rc2.d; ln -s ../init.d/lprng S99lprng)


case "$1" in
  start)
        # Start daemons.
        /bin/echo "Starting lpd: \c"
        ${LPD_PATH}
        /bin/echo ""
        ;;
  stop)
        # Stop daemons.
        /bin/echo "Shutting down lpd: \c"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		/bin/echo " server stopped";
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
        ;;
esac
