dnl
dnl set $(CC) from --with-cc=value
dnl
define(WITH_CC,[
AC_ARG_WITH([cc],
[  --with-cc=COMPILER      select compiler to use])
AC_MSG_CHECKING(for C compiler)
if test "$with_cc" != ""; then
  if test "$ac_cv_prog_cc" != "" && test "$ac_cv_prog_cc" != "$with_cc"; then
    AC_MSG_ERROR(Specified compiler doesn't match cached compiler name;
	remove cache and try again.)
  else
    CC="$with_cc"
  fi
else
AC_PROG_CC
fi
AC_CACHE_VAL(ac_cv_prog_cc,[dnl
  test -z "$CC" && CC=gcc
  AC_TRY_LINK([#include <stdio.h>],[printf("hi\n");], ,
    AC_MSG_ERROR(Can't find a working compiler.))
  ac_cv_prog_cc="$CC"
])
CC="$ac_cv_prog_cc"
AC_MSG_RESULT($CC)
AC_PROG_CC
])dnl
dnl

dnl set $(LD) from --with-linker=value
dnl
define(WITH_LINKER,[
AC_ARG_WITH([linker],
[  --with-linker=LINKER    select linker to use],
AC_MSG_RESULT(LD=$withval)
LD=$withval,
if test -z "$LD" ; then LD=$CC; fi
[AC_MSG_RESULT(LD defaults to $LD)])dnl
AC_SUBST([LD])])dnl

dnl
dnl set $(CCOPTS) from --with-ccopts=value
dnl
define(WITH_CCOPTS,[
AC_ARG_WITH([ccopts],
[  --with-ccopts=CFLAGS    select compiler command line options],
AC_MSG_RESULT(CFLAGS is $withval)
CFLAGS="$withval",
CFLAGS="$CFLAGS"
[AC_MSG_RESULT(CFLAGS defaults to $CFLAGS)]dnl
)dnl
AC_SUBST(CFLAGS)])dnl

dnl
dnl set $(LDFLAGS) from --with-ldopts=value
dnl
define(WITH_LDOPTS,[
AC_ARG_WITH([ldopts],
[  --with-ldopts=LDFLAGS   select linker command line options],
AC_MSG_RESULT(LDFLAGS is $withval)
LDFLAGS=$withval,
LDFLAGS="$LDFLAGS"
[AC_MSG_RESULT(LDFLAGS defaults to $LDFLAGS)]dnl
)dnl
AC_SUBST(LDFLAGS)])dnl

dnl
dnl set $(CPPOPTS) from --with-cppopts=value
dnl
define(WITH_CPPOPTS,[
AC_ARG_WITH([cppopts],
[  --with-cppopts=CPPFLAGS select compiler preprocessor command line options],
AC_MSG_RESULT(CPPFLAGS is $withval)
CPPFLAGS="$withval",
CPPFLAGS="$CPPFLAGS"
[AC_MSG_RESULT(CPPFLAGS defaults to $CPPFLAGS)]
)dnl
AC_SUBST(CPPFLAGS)])dnl


dnl
dnl set $(LOCALEDIR) from --with-localedir=value
dnl
define(WITH_LOCALEDIR,[
AC_ARG_WITH([localedir],
[  --with-localedir=PATH   specify locale information directory],
AC_MSG_RESULT(LOCALEDIR is $withval)
LOCALEDIR="$withval",
LOCALEDIR="$LOCALEDIR"
[AC_MSG_RESULT(LOCALEDIR defaults to $LOCALEDIR)]
)dnl
AC_SUBST(LOCALEDIR)dnl
])dnl


dnl ################################################################
dnl #
dnl #  NLS Support
dnl #
dnl #   The default has been changed to NO nls support.
dnl #   This is done by changing:
dnl #
dnl #    AC_ARG_ENABLE(nls,
dnl #      [  -- disable-nls           do not use Native Language Support],
dnl #      USE_ NLS=$enableval, USE_NLS=yes)
dnl #
dnl #    to:
dnl #
dnl #    AC_ARG_ENABLE(nls,
dnl #      [  --enable-nls           use Native Language Support],
dnl #      USE_NLS=$enableval, USE_NLS=no)
dnl #
dnl #  Add the GETTEXT macros here
dnl #   We depend on having:
dnl #     USE_NLS - set to use nls
dnl #     USE_INCLUDED_LIBINTL
dnl #
dnl #  Put this code after the A M_USE_NLS macro in the configure.in file:
dnl #  if test ${USE_NLS} = yes; then
dnl #    if test -n "${USE_INCLUDED_LIBINTL}" ; then
dnl #       INTLINCS='$(top_builddir)/intl'
dnl #       AC_SUBST(INLINCS)
dnl #    else
dnl #       INTLLIBS=-lintl
dnl #       AC_SUBST(INTLLIBS)
dnl #    fi
dnl #  fi
dnl #
dnl #
dnl ##############################################################
