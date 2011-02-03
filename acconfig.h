/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
#undef _ALL_SOURCE
#endif

/* Define if using ARCHITECTURE  */
#undef ARCHITECTURE

/* Define if using alloca.c.  */
#undef C_ALLOCA

/* Define if type char is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
#undef __CHAR_UNSIGNED__
#endif

/* Define if the closedir function returns void instead of int.  */
#undef CLOSEDIR_VOID

/* Define to empty if the keyword does not work.  */
#undef const

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
#undef CRAY_STACKSEG_END

/* Define for DECALPHA  */
#undef DECALPHA

/* Define for DGUX with <sys/dg_sys_info.h>.  */
#undef DGUX

/* Define if you have <dirent.h>.  */
#undef DIRENT

/* Define if you want to disable force_localhost default  */
#undef DISABLE_FORCE_LOCALHOST

/* Define if you want to disable lpd_bounce default  */
#undef DISABLE_LPD_BOUNCE

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS


/* Define to the type of elements in the array set by `getgroups'.
   Usually this is either `int' or `gid_t'.  */
#undef GETGROUPS_T

/* Define if the `getloadavg' function needs to be run setuid or setgid.  */
#undef GETLOADAVG_PRIVILEGED

/* Define if the `getpgrp' function takes no argument.  */
#undef GETPGRP_VOID

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef gid_t

/* Define if you have alloca, as a function or macro.  */
#undef HAVE_ALLOCA

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#undef HAVE_ALLOCA_H

/* Define if you have a broken TIOCEXCL (IRIX 5) */
#undef HAVE_BROKEN_TIOCEXCL

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define if you don't have vprintf but do have _doprnt.  */
#undef HAVE_DOPRNT

/* Define if <errno.h> contains a declaration for extern int errno */
#undef HAVE_ERRNO_DECL

/* Define if flock definition available */
#undef HAVE_FLOCK_DEF

/* Define if you have gethostname() defined */
#undef HAVE_GETHOSTNAME_DEF

/* Define if your system has its own `getloadavg' function.  */
#undef HAVE_GETLOADAVG

/* Define if you have the getmntent function.  */
#undef HAVE_GETMNTENT

/* Define if you have to use getpgrp(0) to get process group */
#undef HAVE_GETPGRP_0

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define if you have innetgr() defined in include file */
#undef HAVE_INNETGR_DEF

/* Define if you have Kerberos 4 krb_sendauth defined in include file */
#undef HAVE_KRB_AUTH_DEF

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define if the `long double' type works.  */
#undef HAVE_LONG_DOUBLE

/* Define if you support file names longer than 14 characters.  */
#undef HAVE_LONG_FILE_NAMES

/* Define if the `long long' type works.  */
#undef HAVE_LONG_LONG

/* Define if there is lseek proto.  */
#undef HAVE_LSEEK_PROTO

/* Define if you have a working `mmap' system call.  */
#undef HAVE_MMAP

/* Define if you have a openlog prototype.  */
#undef HAVE_OPENLOG_DEF

/* Define if _res defined */
#undef HAVE_RES

/* Define if system calls automatically restart after interruption
   by a signal.  */
#undef HAVE_RESTARTABLE_SYSCALLS

/* Define if you have to use setpgrp(0,0) to set process group */
#undef HAVE_SETPGRP_0

/* Define if you do have setproctitle() defined */
#undef HAVE_SETPROCTITLE_DEF

/* Define if your struct stat has st_blksize.  */
#undef HAVE_ST_BLKSIZE

/* Define if your struct stat has st_blocks.  */
#undef HAVE_ST_BLOCKS

/* Define to 1 if stpcpy function is available.  */
#undef HAVE_STPCPY

/* Define if you have strcasecmp definition  */
#undef HAVE_STRCASECMP_DEF

/* Define if you have the strcoll function and it is properly defined.  */
#undef HAVE_STRCOLL

/* Define if your struct stat has st_rdev.  */
#undef HAVE_ST_RDEV

/* Define if you have the strftime function.  */
#undef HAVE_STRFTIME

/* Define if we have a BSD-ish struct exec in <a.out.h> */
#undef HAVE_STRUCT_EXEC

/* Define if we have a SOLARIS struct linger in <socket.h> */
#undef HAVE_STRUCT_LINGER

/* Define if sys_errlist has the error strings. */
#undef HAVE_SYS_ERRLIST

/* Define if sys_errlist is defined.  */
#undef HAVE_SYS_ERRLIST_DEF

/* Define if syslog() defined */
#undef HAVE_SYSLOG_DEF

/* Define if sys_nerr has number of err strings. */
#undef HAVE_SYS_NERR

/* Define if sys_nerr has definition.  */
#undef HAVE_SYS_NERR_DEF

/* Define if sys_siglist has the signal strings. */
#undef HAVE_SYS_SIGLIST

/* Define if _sys_siglist has the signal strings. */
#undef HAVE__SYS_SIGLIST

/* Define if sys_siglist is defined in unistd.h include file */
#undef HAVE_SYS_SIGLIST_DEF

/* Define if sys_siglist has the signal strings. */
#undef HAVE__SYS_SIGLIST_DEF

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#undef HAVE_SYS_WAIT_H

/* Define if tgetent() has definition  */
#undef HAVE_TGETENT_DEF

/* Define if tgetstr() has definition  */
#undef HAVE_TGETSTR_DEF

/* Define if your struct tm has tm_zone.  */
#undef HAVE_TM_ZONE

/* Define if you don't have tm_zone but do have the external array
   tzname.  */
#undef HAVE_TZNAME

/* Use the "union wait" union to get process status from wait3/waitpid */
#undef HAVE_UNION_WAIT

/* Define if you have <unistd.h>.  */
#undef HAVE_UNISTD_H

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#undef HAVE_UTIME_NULL

/* Define if you have <vfork.h>.  */
#undef HAVE_VFORK_H

/* Define if you have the vprintf function.  */
#undef HAVE_VPRINTF

/* Define if you have the wait3 system call.  */
#undef HAVE_WAIT3

/* HP/UX */
#undef HPUX

/* HP/UX source */
#undef _HPUX_SOURCE

/* Define if the struct in6_addr is defined */
#undef IN6_ADDR

/* Define if the LINUX struct in_addr6 is defined */
#undef IN_ADDR6

/* Define as __inline if that's what the C compiler calls it.  */
#undef inline

/* Define if int is 16 bits instead of 32.  */
#undef INT_16_BITS

/* IRIX */
#undef IRIX

/* Define if long int is 64 bits.  */
#undef LONG_64_BITS

/* Define if major, minor, and makedev are declared in <mkdev.h>.  */
#undef MAJOR_IN_MKDEV

/* Define if major, minor, and makedev are declared in <sysmacros.h>.  */
#undef MAJOR_IN_SYSMACROS

/* Define if on MINIX.  */
#undef _MINIX

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef mode_t


/* Define if you don't have <dirent.h>, but have <ndir.h>.  */
#undef NDIR

/* Define if you have <memory.h>, and <string.h> doesn't declare the
   mem* functions.  */
#undef NEED_MEMORY_H

/* Define if your struct nlist has an n_un member.  */
#undef NLIST_NAME_UNION

/* Define if you have <nlist.h>.  */
#undef NLIST_STRUCT

/* Define if your C compiler doesn't accept -c and -o together.  */
#undef NO_MINUS_C_MINUS_O

/* Define to `long' if <sys/types.h> doesn't define.  */
#undef off_t

/* make autoconf happy */
#undef OSNAME
#undef OSVERSION

/* Define to the name of the distribution.  */
#undef PACKAGE

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef pid_t

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
#undef _POSIX_1_SOURCE

/* Define if you need to in order for stat and other things to work.  */
#undef _POSIX_SOURCE

/* Define if quad_t is NOT present on the system  */
#undef quad_t

/* Define as the return type of signal handlers (int or void).  */
#undef RETSIGTYPE

/* Define if the setvbuf function takes the buffering type as its second
   argument and the buffer pointer as the third, as on System V
   before release 3.  */
#undef SETVBUF_REVERSED

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
#undef size_t

/* Define on SUN Solaris */
#undef SOLARIS

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown
 */
#undef STACK_DIRECTION

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
#undef STAT_MACROS_BROKEN

/* Define if you have the ANSI C header files.  */
#undef STDC_HEADERS

/* Define if you have the ANSI C header files.  */
#undef STDLIB_DEFINES_RANDOM

/* Define if you have struct stat with st_mtime_nsec */
#undef ST_MTIMENSEC
/* Define if you have struct stat with st_mtimespec.ts_nsec */
#undef ST_MTIMESPEC_TV_NSEC

/* Define on SunOS */
#undef SUNOS

/* Define on System V Release 4.  */
#undef SVR4

/* Define if you don't have <dirent.h>, but have <sys/dir.h>.  */
#undef SYSDIR

/* Define if you don't have <dirent.h>, but have <sys/ndir.h>.  */
#undef SYSNDIR

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#undef TIME_WITH_SYS_TIME

/* Define if your <sys/time.h> declares struct tm.  */
#undef TM_IN_SYS_TIME

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef uid_t

/* Define for Encore UMAX.  */
#undef UMAX

/* Define for Encore UMAX 4.3 that has <inq_status/cpustats.h>
   instead of <sys/cpustats.h>.  */
#undef UMAX4_3

/* Define if we are to use the old sgttyb structure and <sgtty.h>. */
#undef USE_SGTTYB

/* Use the statfs structure to find out free space */
#undef USE_STATFS

/* Define how to get file system space */
#undef USE_STATFS_TYPE

/* Use the statvfs structure to find out free space */
#undef USE_STATVFS

/* Define how to change serial line */
#undef USE_STTY

/* Use the statfs() func with f_bfree member to find out free space */
#undef USE_SVR3_STATFS

/* Define if we are to use the termio structure and TCGETA. */
#undef USE_TERMIO

/* Define if we are to use the termios structure and TCGETS. */
#undef USE_TERMIOS

/* Define if we are to use the HP TERMIOX calls */
#undef USE_TERMIOX

/* Use the statfs() func with fs_data member to find out free space */
#undef USE_ULTRIX_STATFS

/* Define if you do not have <strings.h>, index, bzero, etc..  */
#undef USG

/* Define to the version of the distribution.  */
#undef VERSION

/* Define vfork as fork if vfork does not work.  */
#undef vfork

/* Define if the closedir function returns void instead of int.  */
#undef VOID_CLOSEDIR

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#undef WORDS_BIGENDIAN

/* Define if lex declares yytext as a char * by default, not a char[].  */
#undef YYTEXT_POINTER


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
