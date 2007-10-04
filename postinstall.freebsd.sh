#
# -- START --
# postinstall.freebsd.sh,v 1.1 2001/08/21 20:33:16 root Exp
#
#  If you are building a PORT, see the
#  DISTRIBUTIONS/Freebsd directory for a complete port
#  building package.
# 
# This is the shell script that does the postinstall
# dynamic fixup
#  It needs to be massaged with the information for
#  various paths.
# If you are building a package,  then you do NOT want
#  to have this executed - it will put the sample files
#  in place.  You need to do this during the postinstall
#  step in the package installation.
#
echo RUNNING postinstall.freebsd.sh parms "'$0 $@'" MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" INIT="$INIT" cwd `pwd`
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi

fix () {
    v=`echo $1 | sed -e 's/[:;].*//'`;
    p=`echo $2 | sed -e 's/:.*//'`; d=`dirname $p`;
    if expr "$p" : "\|" >/dev/null ; then
        echo "$v is a filter '$p'" 
        return 0
    fi
	n=`basename $p`
    echo "Putting $n in $d, using $v.sample"
    if [ ! -d "$d" ] ; then
        echo "Directory $d does not exist!"
        mkdir -p $d
    fi
	old_version=` echo $p | sed -e "s,/$CONFIG_SUBDIR/,/,"`
	if [ ! -f "$p" -a "$old_version" != "$p" -a -f "$old_version" ] ; then
		echo "WARNING: Location of $p changed from $old_version"
		echo "   Copying $old_version to $p"
		cp "$old_version" "$p" || echo "cannot copy $old_version to $p"
	fi
    if [ -f $v.sample ] ; then
        if [ $v.sample != $p.sample ] ; then ${INSTALL} $v.sample $p.sample; fi
    elif [ -f $v ] ; then
        if [ $v != $p.sample ] ; then ${INSTALL} $v $p.sample; fi
    else
        echo "Do not have $v.sample or $v"
    fi
    if [ ! -f $p.sample ] ; then
        echo "Do not have $p.sample"
    elif [ ! -f $p ] ; then
        ${INSTALL} -m 644 $p.sample $p;
    fi;
}

cnf=${DESTDIR}/etc/rc.conf
startserver(){
	perl -spi -e '
s/^/#/ if /^lpd_enable/;
s/#// if /^#lprng_enable/;
if( /^lprng_enable=/ ){
	s/=.*/=\"YES\"/;
	$found = 1;
}
END {
	print "lprng_enable=\"YES\"\n" if not $found;
}
' ${cnf};
	echo "Stopping LPD"
	killall lpd || true
	sleep 2;
	# check the printcap information
	echo "Checking Printcap Info and fixing permissions"
	${SBINDIR}/checkpc -f || true
	# restart the server
	echo "Restarting server"
	sh $init start || true
}

if [ -f ${cnf} ] ; then
	if grep lprng ${cnf} >/dev/null ; then
		: # no changes
	else
		cat <<EOF >>${cnf}
#to enable LPRng, set lpd_enable="NO" and lprng_enable="YES"
lprng_enable="YES"
EOF
	fi
fi


hold=${DESTDIR}${DATADIR}
echo "Setting up configuration files path for installation" ${hold}
fix ${hold}/lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
fix ${hold}/lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
fix ${hold}/printcap "${DESTDIR}${PRINTCAP_PATH}"

init=${DESTDIR}/usr/local/etc/rc.d/lprng.sh
if [ -n "${INIT}" ] ; then init=${DESTDIR}${INIT}; fi
rm -f $init;
fix ${hold}/lprng.sh $init;
chmod 755 $init;
if [ -n "$STARTSERVER" ] ; then
	startserver;
fi

exit 0
