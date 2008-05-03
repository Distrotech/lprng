define([ENABLE_BOOLEAN],[dnl
AC_ARG_ENABLE([$1],AS_HELP_STRING([$2],[$3]),[v="$enableval"],[v=$4])
AC_MSG_NOTICE([$5])
AS_IF([test "x$v" = "xyes"], [$6], [$7])
])
define([WITH_DIR],[dnl
AC_ARG_WITH([$1],AS_HELP_STRING([$2],[$3]),[$4="$withval"],[$4=$5])
AC_MSG_NOTICE([$6])
AC_SUBST($4)
])
dnl The following code is taken from "po.m4 serial 7 (gettext-0.14.3)"
dnl "gettext.m4 serial 37 (gettext-0.14.4)" and "nls.m4 serial 2 (gettext-0.14.3)"
dnl and mangled heavily to do a bare minimum.
dnl The original files state:
dnl # Copyright (C) 1995-2005 Free Software Foundation, Inc.
dnl # This file is free software; the Free Software Foundation
dnl # gives unlimited permission to copy and/or distribute it,
dnl # with or without modifications, as long as this notice is preserved.
dnl # Authors:
dnl #  Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl #  Bruno Haible <haible@clisp.cons.org>, 2000-2003.
AC_DEFUN([MY_GETTEXT],
[
AC_MSG_CHECKING([whether NLS is requested])
dnl Default is disabled NLS
AC_ARG_ENABLE(nls,AS_HELP_STRING([--enable-nls],[use Native Language Support]),
	USE_NLS=$enableval, USE_NLS=no)
AC_MSG_RESULT($USE_NLS)
AC_SUBST(USE_NLS)
dnl If we use NLS, test it
if test "$USE_NLS" = "yes"; then
        dnl If GNU gettext is available we use this. Fallback to external
	dnl library is not yet supported, but should be easy to request by just
	dnl adding the correct CFLAGS and LDFLAGS to ./configure

        AC_CACHE_CHECK([for GNU gettext in libc], gt_cv_func_gnugettext1_libc,
         [AC_TRY_LINK([#include <libintl.h>
extern int _nl_msg_cat_cntr;
extern int *_nl_domain_bindings;],
            [bindtextdomain ("", "");
return * gettext ("") + _nl_msg_cat_cntr + *_nl_domain_bindings],
            gt_cv_func_gnugettext1_libc=yes,
            gt_cv_func_gnugettext1_libc=no)])
	if test "$gt_cv_func_gnugettext1_libc" = "yes" ; then
		AC_DEFINE(ENABLE_NLS, 1, [Define to 1 if translation of program messages to the user's native language is requested.])
	else
		USE_NLS=no
	fi
fi
AC_MSG_CHECKING([whether to use NLS])
AC_MSG_RESULT([$USE_NLS])
dnl Perform the following tests also without --enable-nls, as
dnl they might be needed to generate the files (for make dist and so on)

dnl Search for GNU msgfmt in the PATH.
dnl The first test excludes Solaris msgfmt and early GNU msgfmt versions.
dnl The second test excludes FreeBSD msgfmt.
AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
  [$ac_dir/$ac_word --statistics /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
   (if $ac_dir/$ac_word --statistics /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
  :)
AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)

dnl Search for GNU xgettext 0.12 or newer in the PATH.
dnl The first test excludes Solaris xgettext and early GNU xgettext versions.
dnl The second test excludes FreeBSD xgettext.
AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
  [$ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1 &&
   (if $ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
  :)
dnl Remove leftover from FreeBSD xgettext call.
rm -f messages.po

dnl Search for GNU msgmerge 0.11 or newer in the PATH.
AM_PATH_PROG_WITH_TEST(MSGMERGE, msgmerge,
  [$ac_dir/$ac_word --update -q /dev/null /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1], :)
])
