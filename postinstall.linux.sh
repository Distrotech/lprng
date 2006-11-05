# 
# -- START --
# postinstall.linux.sh,v 1.1 2001/08/21 20:33:17 root Exp
#
#  If you are building an RPM package,  please see the
#  DISTRIBUTIONS/RPM directory(s) for a RPM Spec file
#  that makes a package
# This script is used from the Makefile when we are doing
#  a source level install and NOT building a package.
# We first install the sample files
#
echo RUNNING postinstall.linux.sh MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" DESTDIR="$DESTDIR" INIT="$INIT" cwd `pwd`
if [ "$VERBOSE_INSTALL" != "" ] ; then set -x; fi
fix () {
	v=`echo $1 | sed -e 's/[:;].*//'`;
    p=`echo $2 | sed -e 's/:.*//'`; d=`dirname $p`;
	if expr "$p" : "\|" >/dev/null ; then
		echo "$v is a filter '$p'" 
		return 0
	fi
	echo "Installing $v.sample in $d as $p"
	if [ ! -d "$d" ] ; then
		echo "Directory $d does not exist!"
		mkdir -p $d;
	fi
	old_version=` echo $p | sed -e "s,/$CONFIG_SUBDIR/,/,"`
	if [ ! -f "$p" -a "$old_version" != "$p" -a -f "$old_version" ] ; then
		echo "WARNING: Location of $p changed from $old_version"
		echo "   Copying $old_version to $p"
		cp "$old_version" "$p" || echo "cannot copy $old_version to $p"
	fi
	if [ -f $v.sample ] ; then
		if [ $v.sample != $p.sample ] ; then ${INSTALL} -m 644 $v.sample $p.sample; fi
	elif [ -f $v ] ; then
		if [ $v != $p.sample ] ; then ${INSTALL} -m 644 $v $p.sample; fi
	else
		echo "Do not have $v.sample or $v"
	fi
	if [ ! -f $p.sample ] ; then
		echo "Do not have $p.sample"
	elif [ ! -f $p ] ; then
		${INSTALL} -m 644 $p.sample $p;
	fi;
}
echo "Installing configuration files"
init=${DESTDIR}/etc/rc.d/init.d/lpd
if [ -n "${INIT}" ] ; then init=${DESTDIR}${INIT}; fi
if [ -f /etc/redhat-release ] ; then
	f=init.redhat;
elif [ -d /lib/lsb ] ; then
	f=init.linuxsb
else
	f=init.linux
fi
if [ "X$MAKEINSTALL" = "XYES" ] ; then
	fix lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
	fix lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
	fix printcap "${DESTDIR}${PRINTCAP_PATH}"
	fix $f `dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd
	if [ "$INIT" != "no" ] ; then
		if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
		${INSTALL} -m 755 $f $init;
	fi;
else
	fix "${LPD_PERMS_PATH}" "${DESTDIR}${LPD_PERMS_PATH}"
	fix "${LPD_CONF_PATH}" "${DESTDIR}${LPD_CONF_PATH}"
	fix "${PRINTCAP_PATH}" "${DESTDIR}${PRINTCAP_PATH}"
	fix $f `dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd
	${INSTALL} -m 755 $f `dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd
fi

if [ "X$MAKEPACKAGE" != "XYES" -a "$INIT" != no ] ; then
    echo "Running startup scripts"
    if [ ! -f $init ] ; then
        echo "Missing $init";
		exit 1
    fi
    if [ -f /sbin/chkconfig ] ; then
		echo "Stopping CUPS server"
		service cups stop || /bin/true
		service cups-lpd stop || /bin/true
		echo "Stopping LPD server"
		service lpr stop || service lprng stop || service lpd stop || /bin/true
		echo "running chkconfig"
		(
		/sbin/chkconfig lpr off
		/sbin/chkconfig lpd off
		/sbin/chkconfig lprng off
		/sbin/chkconfig cups off
		/sbin/chkconfig cups-lpd off
		/sbin/chkconfig `basename $init` off
		)
		echo "Checking Printcap"
		${SBINDIR}/checkpc -f
		echo "Running LPRng Startup Scripts"
		/sbin/chkconfig --add `basename $init`
		/sbin/chkconfig --list `basename $init`
		/sbin/chkconfig `basename $init` on
		echo "Starting Printer"
		service `basename $init` start
		echo "Printer Started"
    else
		echo "Stopping server"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2
		echo "Checking Printcap"
		${SBINDIR}/checkpc -f
		echo "Starting Printer"
		sh `dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd start
		echo "Printer Started"
		m=`dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd
		cat <<EOF
# 
# You will have to install the run time startup files by hand.
# The $m file contains the standard startup/shutdown code.
# You should put this file in the file in common set of rc startup
# scripts and then make symbolic links to it from the run level
# directories.
#
# You can use the following a template for your installation
# 
    initdir=/etc/rc.d
    cp `dirname ${DESTDIR}${LPD_CONF_PATH}`/lpd  \${initdir}/lpd
    ln -s ../init.d/lpd \${initdir}/rc0.d/K60lprng
    ln -s ../init.d/lpd \${initdir}/rc1.d/K60lprng
    ln -s ../init.d/lpd \${initdir}/rc2.d/S60lprng
    ln -s ../init.d/lpd \${initdir}/rc3.d/S60lprng
    ln -s ../init.d/lpd \${initdir}/rc4.d/S60lprng
    ln -s ../init.d/lpd \${initdir}/rc5.d/S60lprng
    ln -s ../init.d/lpd \${initdir}/rc6.d/K60lprng

EOF
	fi;
fi;
