#
# -- START --
# preremove.freebsd.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
# This is the shell script that does the preremove
# lpd shutdown.  It is the script from hell
echo RUNNING preremove.freebsd.sh parms "'$0 $@'"
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
if [ "X$2" = "XDEINSTALL" ] ; then
	echo "Stopping LPD"
	killall -INT lpd
	if [ -f /etc/rc.conf ] ; then
		perl -spi.bak -e '$_ = "" if( /lprng_enable/ );' ${DESTDIR}/etc/rc.conf 
	fi
	init=/usr/local/etc/rc.d/lprng.sh
	if [ -f $init ] ; then
		rm -f $init
	fi
fi
exit 0
