VERSION=3.8.10
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

${RPM}: LPRng.spec
	cp lpd.init /usr/src/redhat/SOURCES/lpd.init
	rpm -ba LPRng.spec

clean:
	rm -f *.tgz
	rm -rf /usr/src/redhat/BUILD/LPRng-*

update:
	perl -spi -e "\
		s|^Version: .*|Version: ${VERSION}|; \
		s|^Release: .*|Release: ${RELEASE}|; \
		" LPRng.spec;

checkit: update
	make rpm
