#
# -- START --
# preremove.freebsd.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
# we shut down the LPD server and remove the startup scripts
#

# we use the /usr/local/etc/rc.d method to start
echo RUNNING preremove.freebsd.sh parms "'$0 $@'"

init=${DESTDIR}/usr/local/etc/rc.d/lprng.sh
if [ -n "${INIT}" ] ; then init=${DESTDIR}${INIT}; fi

if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
echo "Stopping LPD"
killall -INT lpd
#if [ -f /etc/rc.conf ] ; then
#	perl -spi.bak -e '$_ = "" if( /lprng_enable/ );' ${DESTDIR}/etc/rc.conf 
#fi
rm -f $init $init.sample
for i in "${LPD_PERMS_PATH}" "${LPD_CONF_PATH}" "${PRINTCAP_PATH}" ; do
	if diff "$i" "$i.sample" >/dev/null ; then
		# no changes
		rm -f $i
	fi
	rm -f "$i.sample"
done
exit 0
