VERSION=3.7.3
RELEASE=1
RPM=/usr/src/redhat/RPMS/i386/LPRng-${VERSION}-${RELEASE}.i386.rpm
CONFIGURE_ARGS=--prefix=/usr --sysconfdir=/etc --with-filterdir=/usr/libexec/filters --with-ld_library_path=/lib:/usr/lib:/usr/local/lib --with-filter_path=/bin:/usr/bin:/usr/libexec:/usr/local/libexec:/usr/sbin:/var/spool/bin

COPY_HERE=
.PHONY: all rpm testi testu dou doU undo update clean checkit

all:
	make update
	make rpm

rpm:	${RPM}

testi:
	rpm -i --test ${RPM}
testu:
	rpm -U --test ${RPM}
dou:
	rpm -U  ${RPM}
doU:
	rpm -U --force ${RPM}
undo:
	rpm -e --nodeps LPRng

${RPM}: LPRng_rh.spec
	rpm -ba LPRng_rh.spec

LPRng_rh.spec: preremove postinstall
	-rm -f LPRng_rh.spec
	cat LPRng_rh.spec.header >LPRng_rh.spec
	echo "%preun"   >>LPRng_rh.spec
	cat preremove   >>LPRng_rh.spec
	echo "%post"    >>LPRng_rh.spec
	cat postinstall >>LPRng_rh.spec

preremove postinstall : LPRng_rh.spec.header
	-rm -f preremove postinstall
	COPY_HERE=`pwd`; export COPY_HERE ; rpm -bc LPRng_rh.spec.header
clean:
	rm -f LPRng_rh.spec *.bak postinstall preremove files *.tgz
	rm -rf /usr/src/redhat/BUILD/LPRng-*

update:
	perl -spi -e "\
		s|^\%define version.*|\%define version ${VERSION}|; \
		s|^\%define release.*|\%define release ${RELEASE}|; \
		" LPRng_rh.spec.header;

checkit: update
	make rpm
