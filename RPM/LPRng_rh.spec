#							-*-shell-script-*-
# LPRng Spec File
#
%define name LPRng
%define version 3.6.10
%define release 1

Summary: LPRng Print Spooler for UNIX and NT
Name: %{name}
Version: %{version}
Release: %{release}

Copyright: OpenSource
Group: System Environment/Daemons
Source: ftp://ftp.astart.com/pub/LPRng/LPRng/%{name}-%{version}.tgz
URL: http://www.astart.com/LPRng.html
Vendor: Astart Technologies, San Diego, CA 92123 http://www.astart.com
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root
Packager: Patrick Powell <papowell@astart.com>

%description
The LPRng software is an enhanced, extended, and portable implementation
of the Berkeley LPR print spooler functionality. While providing the
same interface and meeting RFC1179 requirements, the implementation
is completely new and provides support for the following features:
lightweight (no databases needed) lpr, lpc, and lprm programs; dynamic
redirection of print queues; automatic job holding; highly verbose
diagnostics; multiple printers serving a single queue; client programs
do not need to run SUID root; greatly enhanced security checks; and a
greatly improved permission and authorization mechanism.

The source software compiles and runs on a wide variety of UNIX systems,
and is compatible with other print spoolers and network printers that
use the LPR interface and meet RFC1179 requirements.  LPRng provides
emulation packages for the SVR4 lp and lpstat programs, eliminating the
need for another print spooler package. These emulation packages can be
modified according to local requirements, in order to support vintage
printing systems.  An NT version is also available.

For users that require secure and/or authenticated printing support,
LPRng supports Kerberos V, MIT Kerberos IV Print Support, and PGP
authentication.  LPRng is being adopted by MIT for use as their Campus
Wide printing support system. Additional authentication support is
extremely simple to add.  LPRng is Open Source Software, and the current
public distribution is available from the listed FTP and Web Sites.

%changelog
* Sat Sep  4 1999 Patrick Powell <papowell@astart.com>
  - did ugly things to put the script in the spec file
* Sat Aug 28 1999 Giulio Orsero <giulioo@tiscalinet.it>
  - 3.6.8
* Fri Aug 27 1999 Giulio Orsero <giulioo@tiscalinet.it>
  - 3.6.7 First RPM build.
  
%prep

%setup

%build
CFLAGS=$RPM_OPT_FLAGS  ./configure --prefix=$RPM_BUILD_ROOT/usr --sysconfdir=$RPM_BUILD_ROOT/etc
make 

# clean docs	
cd $RPM_BUILD_DIR/%{name}-%{version}/HOWTO;
rm -f Make* fix* table* update* *.sgml *.info
gzip -9 LPRng-HOWTO.txt

%install

rm -rf $RPM_BUILD_ROOT

# ---- WARNING ---- READ THIS BEFORE YOU MODIFY THE SCRIPT
# The next section creates the lpd startup/shutdown script.
# This exposes ONE (and there are many many more)  of the major botches in
# the RPM design and implementation - the assumption that
# WLOG that there will be a package of files shipped around with the script,
# which will get carefully installed in the various RPM build directories,  just
# in time to have them carelessly overwritten by some clod that assumes that
# he can use the same name.  This is very poor software engineering, and I am
# not impressed with this sort of arrogant assumption by the package designers.
#  Patrick Powell <papowell@astart.com>
#
#  if you plan to modify this then:
#  a) ensure that the script and other files have timestamps, versions or other
#     indications that relate them to the installed version of this software
#  b) put them up at a site where they can be accessed.
#  c) mirror them to the mirror sites...
#  
#  After considering the options,  I am generating all the auxillary files as part
#  of the spec file.
#######

cat <<'EOF' >/tmp/lpd
#!/bin/sh
#
# lpd           This shell script takes care of starting and stopping
#               lpd (printer daemon).
#
# chkconfig: 2345 60 60
# description: lpd is the print daemon required for lpr to work properly. \
#   It is basically a server that arbitrates print jobs to printer(s).
# processname: lpd
# config: /etc/printcap

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/network

# Check that networking is up.
[ ${NETWORKING} = "no" ] && exit 0

[ -f /usr/sbin/lpd ] || exit 0

[ -f /etc/printcap ] || exit 0

# See how we were called.
case "$1" in
  start)
        # Start daemons.
        echo -n "Starting lpd: "
        daemon lpd
        echo
        touch /var/lock/subsys/lpd
        ;;
  stop)
        # Stop daemons.
        echo -n "Shutting down lpd: "
	killproc lpd
        echo
        rm -f /var/lock/subsys/lpd
        ;;
  status)
	status lpd
	;;
  restart|reload)
	$0 stop
	$0 start
	;;
  *)
        echo "Usage: lpd {start|stop|restart|reload|status}"
        exit 1
esac

exit 0
EOF


mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc0.d/
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc1.d/
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc2.d/
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc3.d/
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc5.d/
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/rc6.d/
make install

# rhl init script
install -m 755 /tmp/lpd $RPM_BUILD_ROOT/etc/rc.d/init.d/lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc0.d/K60lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc1.d/K60lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc2.d/S60lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc3.d/S60lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc5.d/S60lpd
ln -s ../init.d/lpd  $RPM_BUILD_ROOT/etc/rc.d/rc6.d/K60lpd

%clean
rm -rf $RPM_BUILD_ROOT

%pre
if test -x /etc/rc.d/init.d/lpd
	then /etc/rc.d/init.d/lpd stop 1>/dev/null 2>&1
fi

%post
/sbin/chkconfig --add lpd

%preun
/etc/rc.d/init.d/lpd stop 1>/dev/null 2>&1
if test "$1" = 0
then
	/sbin/chkconfig --del lpd
fi

%files
%defattr(-,root,root)
%attr(644,root,root) %config(noreplace) /etc/lpd.conf
%attr(644,root,root) %config(noreplace) /etc/lpd.perms
%attr(644,root,root) /etc/lpd.conf.sample
%attr(644,root,root) /etc/lpd.perms.sample
%doc ABOUT-NLS.LPRng ANNOUNCE CHANGES CONTRIBUTORS COPYRIGHT INSTALL LICENSE 
%doc README* VERSION Y2KCompliance HOWTO
%docdir HOWTO
%attr(755,root,root)  /etc/rc.d/init.d/lpd
%attr(-,root,root)  /etc/rc.d/rc*.d/*60lpd
%attr(4755,root,root) /usr/bin/lpq
%attr(4755,root,root) /usr/bin/lprm
%attr(4755,root,root) /usr/bin/lpr
%attr(4755,root,root) /usr/bin/lpstat
%attr(-,root,root)  /usr/bin/lp
%attr(-,root,root)  /usr/bin/cancel
%attr(4755,root,root) /usr/sbin/lpc
%attr(755,root,root)  /usr/sbin/lpd
%attr(755,root,root)  /usr/sbin/checkpc
%attr(755,root,root)  /usr/sbin/lpraccnt
%attr(755,root,root)  /usr/libexec/filters/*
/usr/man/*/*
# end of SPEC file
