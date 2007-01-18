#!/bin/sh
set -e
rm -f aclocal.m4 libtool config.cache config.status
echo "Running aclocal..."
aclocal
echo "Running autoconf..."
autoconf
ed -s Makefile.am <<EOF
g/# EVERYTHING AFTER THIS LINE IS DELETED BY autogen.sh BEFORE GETTEXT IS RUN AGAIN:/+1,$d
w
q
EOF
echo "Running gettextize..."
echo "(This will most likely demand some things that can be ignored)"
echo "" > /dev/tty ; gettextize --force --copy --no-changelog < /dev/null
rm -f po/Makevars
mv po/Makevars.template po/Makevars
perl -spi.bak -e 's/AC_OUTPUT\( po\/Makefile.in/AC_OUTPUT(/;' configure.ac
perl -spi.bak -e '
	if(/--disable-nls.*Native/){s/--disable/--enable/; s/do not //;}
	s/, USE_NLS=yes/, USE_NLS=no/; ' aclocal.m4
echo "Running autoconf..."
autoconf
echo "Running autoheader..."
autoheader
echo "Running automake..."
automake --foreign -a -c
echo "Now you can run ./configure"
echo "(use --enable-maintainer-mode if you want Makefile.in automatically regenerated)"
