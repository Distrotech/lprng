#
# -- START --
# $Id: postinstall.bsdi.sh,v 1.12 2000/10/29 22:52:48 papowell Exp papowell $
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

echo RUNNING postinstall.bsdi MAKEPACKAGE="$MAKEPACKAGE" MAKEINSTALL="$MAKEINSTALL" PREFIX="$PREFIX" INIT="$INIT" cwd `pwd`
fix () {
	v=`echo $1 | sed -e 's/[:;].*//'`;
	p=`echo $2 | sed -e 's/[:;].*//'`; d=`dirname $p`;
	if expr "$p" : "\|" >/dev/null ; then
		echo "$v is a filter '$p'" 
		exit 0
	fi
	echo "Checking for $v.sample in $d"
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
echo "Installing configuration files"
fix lpd.perms "${DESTDIR}${LPD_PERMS_PATH}"
fix lpd.conf "${DESTDIR}${LPD_CONF_PATH}"
fix printcap "${DESTDIR}${PRINTCAP_PATH}"
init=${DESTDIR}/usr/local/etc/rc.d/lprng.sh
if [ "$INIT" != "" ] ; then
	init=${DESTDIR}$INIT;
fi
if [ "$INIT" != "no" ] ; then
	if [ ! -d `dirname $init` ] ; then mkdir -p `dirname $init ` ; fi;
	cp init.bsdi $init
	chmod 755 $init
	if test "$MAKEPACKAGE" != "YES" ; then
		# restart the printer
		echo "Stopping LPD"
		kill -INT `ps ${PSHOWALL} | awk '/lpd/{ print $1;}'` >/dev/null 2>&1
		sleep 2
		echo "Checking Printcap Info and fixing permissions"
		${SBINDIR}/checkpc -f
		# restart the server
		echo "Restarting server"
		$init start
		cat <<EOF

You need to put 'sh $init start'
in your /etc/rc.local file or modify rc.local
to start scripts in the /usr/local/etc/rc.d file.
  Example: 
EOF
		cat <<'EOF'
# set the local_startup to have the directories with startup scripts
local_startup="/usr/local/etc"
# put this in /etc/rc or /etc/rc.local
case ${local_startup} in
[Nn][Oo] | '') ;;
*)
    echo -n 'Local package initialization:'
    for dir in ${local_startup}; do
        if [ -d "${dir}" ]; then
            for script in ${dir}/*.sh; do
                if [ -x "${script}" ]; then
                    (set -T; trap 'exit 1' 2; ${script} start)
                fi
            done
        fi
    done
    ;;
esac
EOF
	fi
fi
exit 0
