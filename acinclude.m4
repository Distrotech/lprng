define([ENABLE_BOOLEAN],[dnl
AC_ARG_ENABLE([$1],[$2],[v="$enableval"],[v=$3])
AC_MSG_NOTICE([$4])
AS_IF([test "x$v" = "xyes"], [$5], [$6])
])
define([WITH_DIR],[dnl
AC_ARG_WITH([$1],[$2],[$3="$withval"],[$3=$4])
AC_MSG_NOTICE([$5])
AC_SUBST($3)
])
