#
# -- START --
# $Id: postinstall.freebsd.sh,v 1.12 2000/10/29 22:52:48 papowell Exp papowell $
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
fix () {
	v=`echo $1 | sed -e 's/[:;].*//'`;
    p=`echo $2 | sed -e 's/:.*//'`; d=`dirname $p`;
	if expr "$p" : "\|" >/dev/null ; then
		echo "$v is a filter '$p'" 
		exit 0
	fi
    echo "Putting $p in $d, using $v.sample"
	if [ ! -d "$d" ] ; then
		echo "Directory $d does not exist!"
		mkdir -p $d
	fi
	if [ -f $v.sample ] ; then
		if [ $v.sample != $p.sample ] ; then cp $v.sample $p.sample; fi
	elif [ -f $v ] ; then
		if [ $v != $p.sample ] ; then cp $v $p.sample; fi
	else
		echo "Do not have $v.sample or $v"
	fi
	if [ ! -f $p.sample ] ; then
		echo "Do not have $p.sample"
	elif [ ! -f $p ] ; then
		chmod 644 $p.sample
		cp $p.sample $p;
		chmod 644 $p;
	fi;
}
# we use the /usr/local/etc/rc.d method to start
# lpd
# we have to take them from one place and put in another
if [ "X$MAKEPACKAGE" = "XYES" ] ; then
	hold=${PREFIX}/etc
	echo "Setting up configuration files path" ${hold}
	# we put files into the destination
    if [ ! -d ${hold} ] ; then mkdir -p ${hold} ; fi;
    cp lpd.perms ${hold}/lpd.perms.sample
    cp lpd.conf ${hold}/lpd.conf.sample
    cp printcap ${hold}/printcap.sample
	if [ "$INIT" != no ] ; then
		cp init.freebsd ${hold}/lprng.sh
	fi
elif [ "X$MAKEINSTALL" = XYES ] ; then
	hold=${DESTDIR}${PREFIX}/etc
	echo "Setting up configuration files path" ${hold}
    if [ ! -d ${hold} ] ; then mkdir -p ${hold} ; fi;
    cp lpd.perms ${hold}/lpd.perms.sample
    cp lpd.conf ${hold}/lpd.conf.sample
    cp printcap ${hold}/printcap.sample

    fix ${hold}/lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
    fix ${hold}/lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
    fix ${hold}/printcap "${DESTDIR}${PRINTCAP_PATH}"

	if [ "$INIT" != no ] ; then
		cp init.freebsd ${hold}/lprng.sh
		init=${DESTDIR}/usr/local/etc/rc.d/lprng.sh
		if [ "$INIT" != "" ] ; then
			init=${DESTDIR}$INIT;
		fi
		echo "Setting up init script $init using init.freebsd"
		if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
		cp ${hold}/lprng.sh $init
		chmod 744 $init

		echo "Stopping LPD"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2;
		# check the printcap information
		echo "Checking Printcap Info and fixing permissions"
		${SBINDIR}/checkpc -f
		# restart the server
		echo "Restarting server"
		sh $init start
	fi
elif [ "X$2" = "XPOST-INSTALL" ] ; then
	# when doing an install from a package we get the file from the package
	hold=etc
    if [ -f ${hold}/lpd.perms.sample ] ; then
        fix ${hold}/lpd.perms "${LPD_PERMS_PATH}"
        fix ${hold}/lpd.conf "${LPD_CONF_PATH}"
        fix ${hold}/printcap "${PRINTCAP_PATH}"
		init=/usr/local/etc/rc.d/lprng.sh
		if [ "$INIT" != "" ] ; then
			init=$INIT;
		fi
		if [ -f $init -a "$INIT" != "no" ] ; then
			cp ${hold}/lprng.sh $init
			chmod 744 $init
		fi
    else
        echo "WARNING: configuration files missing from package! CWD " `pwd`
		ls
		exit 1
    fi
fi
exit 0
