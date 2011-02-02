#!/bin/sh
set -e
rm -f aclocal.m4 libtool config.cache config.status
echo "Running aclocal..."
aclocal
echo "Running autoconf..."
autoconf
echo "Running autoheader..."
autoheader
echo "Running automake..."
automake --foreign -a -c
for dir in .  UTILS man src po conf ; do
	if test -f $dir/.cvsignore.example ; then
		if test -e $dir/.cvsignore ; then
			:
		else
			cp $dir/.cvsignore.example $dir/.cvsignore
		fi
	fi
done
echo "Now you can run ./configure"
echo "(use --enable-maintainer-mode if you want Makefile.in automatically regenerated)"
