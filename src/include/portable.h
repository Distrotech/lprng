/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: portable.h
 * PURPOSE:
 * The configure program generates config.h,  which defines various
 * macros indicating the presence or abscence of include files, etc.
 * However, there are some systems which pass the tests,  but things
 * do not work correctly on them.  This file will try and fix
 * these things up for the user.
 *
 * NOTE:  if there were no problems, this file would be:
 *    #include "config.h"
 *
 * Sigh. Patrick Powell Thu Apr  6 07:00:48 PDT 1995 <papowell@sdsu.edu>
 *    NOTE: thanks to all the folks who worked on the PLP software,
 *    Justin Mason <jmason@iona.ie> especially.  Some of the things
 *    that you have to do to get portability are truely bizzare.
 *
 * $Id: portable.h,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $
 **************************************************************************/

#ifndef _PLP_PORTABLE_H
#define _PLP_PORTABLE_H 1

#ifndef __STDC__
LPRng requires ANSI Standard C compiler
#endif

#include "config.h"

/*************************************************************************
 * ARGH: some things that "configure" can't get right.
*************************************************************************/

/***************************************************************************
 * porting note: if you port PLP and you get some errors
 * caused by autoconf guessing the wrong set of functions/headers/structs,
 * add or change the entry for your system in the ARGH section below.
 * You might want to try and determine how your system is identified
 * by the C preprocessor and use this informaton rather than trying
 * to look for information in various f1les.
 *    Patrick Powell and Justin Mason
 ***************************************************************************/

/*************************************************************************
 * APOLLO Ports
 *  Thu Apr  6 07:01:51 PDT 1995 Patrick Powell
 * This appears to be historical.
 *************************************************************************/
#ifdef apollo
# define IS_APOLLO
/* #undef __STDC__ */
/* # define CONFLICTING_PROTOS */
#endif

/*************************************************************************
 * ULTRIX.
 * Patrick Powell Thu Apr  6 07:17:34 PDT 1995
 * 
 * Take a chance on using the standard calls
 *************************************************************************/
#ifdef ultrix
# define IS_ULTRIX
#endif


/*************************************************************************
 * AIX.
 *************************************************************************/
#ifdef _AIX32 
# define IS_AIX32
#endif

/*************************************************************************
 * Sun
 *************************************************************************/

#if defined(sun)
#endif


/*************************************************************************/
#if defined(NeXT)
# define IS_NEXT
# define __STRICT_BSD__
#endif

/*************************************************************************/
#if defined(__sgi) && defined(_SYSTYPE_SVR4)
# define IS_IRIX5
#endif

/*************************************************************************/
#if defined(__sgi) && defined(_SYSTYPE_SYSV)
#define IS_IRIX4
#endif

/*************************************************************************/
#if defined(__linux__) || defined (__linux) || defined (LINUX)
# define IS_LINUX
#endif


/*************************************************************************/
#if defined(__hpux) || defined(_HPUX_SOURCE)
# define IS_HPUX
# undef _HPUX_SOURCE
# define _HPUX_SOURCE 1
#endif
  
/*************************************************************************/

#if defined(__convex__) /* Convex OS 11.0 - from w_stef */
# define IS_CONVEX
# define LPASS8 (L004000>>16)
#endif

/*************************************************************************/

#ifdef _AUX_SOURCE
# define IS_AUX
# define _POSIX_SOURCE

# undef HAVE_GETPGRP_0
# undef SETPROCTITLE

#endif

/*************************************************************************/

#if defined(SNI) && defined(sinix)
# define IS_SINIX
#endif


/*************************************************************************/
#if defined(__svr4__) && !defined(SVR4)
# define SVR4 __svr4__
#endif

/***************************************************************************
 * Solaris SUNWorks CC compiler
 *  man page indicates __SVR4 is defined, as is __unix, __sun
 ***************************************************************************/
#if (defined(__SVR4) || defined(_SVR4_)) && !defined(SVR4)
# define SVR4 1
#endif

/*************************************************************************/
#if defined(__bsdi__)
# define IS_BSDI
#endif

/*************************************************************************/

/*************************************************************************
 * we also need some way of spotting IS_DATAGEN (Data Generals),
 * and IS_SEQUENT (Sequent machines). Any suggestions?
 * these ports probably don't work anymore...
 *************************************************************************/

/*************************************************************************
 * END OF ARGH SECTION; next: overrides from the Makefile.
 *************************************************************************/
/*************************
 * STTY functions to use *
 *************************/
#define SGTTYB  0
#define TERMIO  1
#define TERMIOS 2

/*************************
 * FSTYPE functions to use *
 *************************/

#define SVR3_STATFS       0
#define ULTRIX_STATFS     1
#define STATFS            2
#define STATVFS           3

#if defined(MAKE_USE_STATFS)
# undef USE_STATFS
# define USE_STATFS MAKE_USE_STATFS
#endif

#if defined(MAKE_USE_STTY)
# undef  USE_STTY
# define USE_STTY MAKE_USE_STTY 
#endif


/*********************************************************************
 * GET STANDARD INCLUDE FILES
 * This is the one-size-fits-all include that should grab everthing.
 * This has a horrible impact on compilation speed,  but then, do you
 * want compilation speed or portability?
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************
 * If you do not have the following, you are doomed. Or at least
 * going to have an uphill hard time.
 * NOTE: string.h might also be strings.h on some very very odd systems
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#include <errno.h>
#include <grp.h>
/*********************************************************************
 * yuck -- this is a nightmare! half-baked-ANSI systems are poxy (jm)
 *
 * Note that configure checks for absolute compliance, i.e.-
 * older versions of SUNOS, HP-UX, do not meet this.
 *
 * Patrick Powell Thu Apr  6 07:21:10 PDT 1995
 *********************************************************************/


#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
  extern int dup2 ();
  extern int execve ();
  extern uid_t geteuid (), getegid ();
  extern int setgid (), getgid ();
#endif


#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#else
  char *getenv( char * );
  void abort(void);
#endif

#ifndef HAVE_STRCHR
# define strchr			index
# define strrchr		rindex
#endif

/*********************************************************************
 * directory management is nasty.  There are two standards:
 * struct directory and struct dirent.
 * Solution:  macros + a typedef.
 * Patrick Powell Thu Apr  6 07:44:50 PDT 1995
 *
 *See GNU autoconf documentation for this little AHEM gem... and others
 *  too obnoxious to believe
 *********************************************************************/

#if HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NLENGTH(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

typedef struct dirent plp_dir_t;

/*********************************************************************
 * malloc strikes again. Definition is a la ANSI C.  However,
 * You may need to edit this on historical systems.
 * Patrick Powell Thu Apr  6 07:47:54 PDT 1995
 *********************************************************************/

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#else
# if !defined(HAVE_STDLIB_H)
   void *malloc(size_t);
   void free(void *);
# endif
#endif

#ifndef HAVE_ERRNO_DECL
 extern int errno;
#endif

/*********************************************************************
 * Note the <time.h> may already be included by some previous
 * lines.  You may need to edit this by hand.
 * Better solution is to put include guards in all of the include files.
 * Patrick Powell Thu Apr  6 07:55:58 PDT 1995
 *********************************************************************/
 
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
# endif
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

/*
 * we use the FCNTL code if we have it
 * We want you to define F_SETLK, etc.  If they are not defined,
 *  Then you better put a system dependent configuration
 *  in and define them.
 */
#if defined(HAVE_FCNTL) && ! defined(F_SETLK)
/*ABORT: "defined(HAVE_FCNTL) && ! defined(F_SETLK)"*/
#undef HAVE_FCNTL
#endif

#if defined(HAVE_LOCKF) && ! defined(F_LOCK)
/*ABORT: "defined(HAVE_LOCKF) && ! defined(F_LOCK)"*/
/* You must fix this up */
#undef HAVE_LOCKF
#endif

#if defined(HAVE_FLOCK) && ! defined(LOCK_EX)
/*AB0RT: "defined(HAVE_FLOCK) && ! defined(LOCK_EX)"*/
/* You must fix this up */
#undef HAVE_FLOCK
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* 4.2 BSD systems */
#ifndef S_IRUSR
# define S_IRUSR S_IREAD
# define S_IWUSR S_IWRITE
# define S_IXUSR S_IEXEC
# define S_IXGRP (S_IEXEC >> 3)
# define S_IXOTH (S_IEXEC >> 6)
#endif

#ifndef S_ISLNK
# define S_ISLNK(mode) (((mode) & S_IFLNK) == S_IFLNK)
#endif
#ifndef S_ISREG
# define S_ISREG(mode) (((mode) & S_IFREG) == S_IFREG)
#endif
#ifndef S_ISDIR
# define S_ISDIR(mode) (((mode) & S_IFDIR) == S_IFDIR)
#endif


/* 4.2 BSD systems */
#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

#ifndef HAVE_KILLPG
# define killpg(pg,sig)	((int) kill ((pid_t)(-(pg)), (sig)))
#endif

/***********************************************************************
 * wait() stuff: most recent systems support a compatability version
 * of "union wait", but it's not as fully-featured as the recent stuff
 * that uses an "int *". However, we want to keep support for the
 * older BSD systems as much as possible, so it's still supported;
 * however, if waitpid() exists, we're POSIX.1 compliant, and we should
 * not use "union wait". (hack hack hack) (jm)
 *
 * I agree.  See the waitchild.c code for a tour through the depths of
 * portability hell.
 *
 * Patrick Powell Thu Apr  6 08:03:58 PDT 1995
 *
 ***********************************************************************/

#ifdef HAVE_WAITPID
# undef HAVE_UNION_WAIT		/* and good riddance */
#endif

/***************************************************************************
 * HAVE_UNION_WAIT will be def'd by configure if it's in <sys/wait.h>,
 * and isn't just there for compatibility (like it is on HP/UX).
 ***************************************************************************/

#ifdef HAVE_UNION_WAIT
typedef union wait		plp_status_t;
/*
 * with some BSDish systems, there are already #defines for this,
 * so we should use them if they're there.
 */
# ifndef WCOREDUMP
#  define WCOREDUMP(x)	((x).w_coredump)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(x)	((x).w_retcode)
# endif
# ifndef WTERMSIG
#  define WTERMSIG(x)	((x).w_termsig)
# endif
# ifndef WIFSTOPPED
#  define WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(x)	((x).w_stopval == WEXITED)
# endif

#else
  typedef int			plp_status_t;
/* The POSIX defaults for these macros. (this is cheating!) */
# ifndef WTERMSIG
#  define WTERMSIG(x)	((x) & 0x7f)
# endif
# ifndef WCOREDUMP
#  define WCOREDUMP(x)	((x) & 0x80)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(x)	((((unsigned) x) >> 8) & 0xff)
# endif
# ifndef WIFSIGNALED
#  define WIFSIGNALED(x)	(WTERMSIG (x) != 0)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(x)	(WTERMSIG (x) == 0)
# endif
#endif /* HAVE_UNION_WAIT */

/***********************************************************************
 * SVR4: SIGCHLD is really SIGCLD; #define it here.
 * PLP lpd _does_ handle the compatibility semantics properly
 * (Advanced UNIX Programming p. 281).
 ***********************************************************************/

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD			SIGCLD
#endif


/***********************************************************************
 * configure will set RETSIGTYPE to the type returned by signal()
 ***********************************************************************/

typedef RETSIGTYPE plp_signal_t;

#ifndef HAVE_GETDTABLESIZE
# ifdef NOFILE
#  define getdtablesize()	NOFILE
# else
#  ifdef NOFILES_MAX
#   define getdtablesize()	NOFILES_MAX
#  endif
# endif
#endif


#if !defined(NDEBUG)
# ifdef HAVE_ASSERT_H
#  include <assert.h>
# else
#  ifdef HAVE_ASSERT
    extern void assert();
#  else
#   define assert(X) if(!(X)){abort(); /* crash and burn */ }
#  endif
# endif
#else
# define assert(X) /* empty */
#endif

#ifndef HAVE_STRDUP
# ifdef __STDC__
   char *strdup(const char*);
# else
   char *strdup();
# endif
#endif

#ifndef IPPORT_RESERVED
#define IPPORT_RESERVED 1024
#endif


/* varargs declarations: */

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
# define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
# define VA_LOCAL_DECL   va_list ap;
# define VA_START(f)     va_start(ap, f)
# define VA_SHIFT(v,t)	;	/* no-op for ANSI */
# define VA_END          va_end(ap)
#else
# if defined(HAVE_VARARGS_H)
#  include <varargs.h>
#  undef HAVE_STDARGS
#  define VA_LOCAL_DECL   va_list ap;
#  define VA_START(f)     va_start(ap)		/* f is ignored! */
#  define VA_SHIFT(v,t)	v = va_arg(ap,t)
#  define VA_END		va_end(ap)
# else
XX ** NO VARARGS ** XX
# endif
#endif

#if !defined(IS_ULTRIX) && defined(HAVE_SYSLOG_H)
# include <syslog.h>
#endif
#if defined(HAVE_SYS_SYSLOG_H)
# include <syslog.h>
#endif
# if defined(LOG_PID) && defined(LOG_NOWAIT)
#  define HAVE_OPENLOG
# endif /* LOG_PID && LOG_NOWAIT */

/*
 *  Priorities (these are ordered)
 */
#ifndef LOG_ERR
# define LOG_EMERG   0   /* system is unusable */
# define LOG_ALERT   1   /* action must be taken immediately */
# define LOG_CRIT    2   /* critical conditions */
# define LOG_ERR     3   /* error conditions */
# define LOG_WARNING 4   /* warning conditions */
# define LOG_NOTICE  5   /* normal but signification condition */
# define LOG_INFO    6   /* informational */
# define LOG_DEBUG   7   /* debug-level messages */
#endif

#ifdef LOG_LPR
# define SYSLOG_FACILITY LOG_LPR
#else
# ifdef LOG_LOCAL0
#  define SYSLOG_FACILITY LOCAL0
# else
#  define SYSLOG_FACILITY (0) /* for Ultrix -- facilities aren't supported */
# endif
#endif

/*************************************************************************
 * If we have SVR4 and no setpgid() then we need getpgrp() 
 *************************************************************************/
#if defined(SVR4) || defined(__alpha__)
# undef HAVE_GETPGRP_0
# undef HAVE_SETPGRP_0
#endif

/*
 * NONBLOCKING Open and IO - different flags for
 * different systems
 */

#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif


#define NONBLOCK (O_NDELAY|O_NONBLOCK)
#ifdef IS_HPUX
#undef NONBLOCK
#define NONBLOCK (O_NONBLOCK)
#endif


/*********************************************************************
 * AIX systems need this
 *********************************************************************/

#if defined(HAVE_SYS_SELECT_H)
# include <sys/select.h>
#endif

#endif	/* PLP_PORTABLE_H */
