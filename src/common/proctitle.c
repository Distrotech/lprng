/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: proctitle.c
 * PURPOSE: put a process title in place
 **************************************************************************/

static char *const _id = "$Id: proctitle.c,v 3.1 1996/12/28 21:40:18 papowell Exp $";

#include "lp.h"
/**** ENDINCLUDE ****/

/*
 *  SETPROCTITLE -- set process title for ps
 *  proctitle( char *str );
 * 
 *	Returns: none.
 * 
 *	Side Effects: Clobbers argv of our main procedure so ps(1) will
 * 		      display the title.
 */

#if !defined(HAVE_SETPROCTITLE)

#ifdef HAVE_SYS_PSTAT_H
# include <sys/pstat.h>
#endif
#ifdef HAVE_VMPARAM_H
# include <machine/vmparam.h>
#endif
#ifdef HAVE_SYS_EXEC_H
# include <sys/exec.h>
#endif
#if defined(__bsdi__) || defined (__FreeBSD__)
# undef PS_STRINGS    /* BSDI 1.0 doesn't do PS_STRINGS as we expect */
# define PROCTITLEPAD	'\0'
#endif
#ifdef __FreeBSD__
# undef PS_STRINGS	/* XXX This is broken due to needing<machine/pmap.h> */
# define PROCTITLEPAD	'\0'
#endif
#ifdef PS_STRINGS
# define SETPROC_STATIC static
#endif
#ifndef SETPROC_STATIC
# define SETPROC_STATIC
#endif
#ifndef PROCTITLEPAD
# define PROCTITLEPAD	' '
#endif

extern char **Argv_p;
extern int Argc_p;

/* VARARGS1 */
void
#ifdef HAVE_STDARGS
setproctitle (const char *fmt,...)
#else
setproctitle (va_alist) va_dcl
#endif
{
#ifndef NO_SETPROCTITLE
    int i, len;
	char *end;
    SETPROC_STATIC char buf[LINEBUFFER];
# ifdef _HPUX_SOURCE
    union pstun pst;
# endif
    VA_LOCAL_DECL
# ifndef HAVE_STDARGS
    const char *fmt;
# endif

    /* print the argument string */
    VA_START (fmt);
    VA_SHIFT (fmt, char *);
    (void) vplp_snprintf(buf, sizeof(buf), fmt, ap);
    VA_END;

    i = strlen (buf);

# ifdef _HPUX_SOURCE
    pst.pst_command = buf;
    pstat (PSTAT_SETCMD, pst, i, 0, 0);
# else
#  ifdef PS_STRINGS
    PS_STRINGS->ps_nargvstr = 1;
    PS_STRINGS->ps_argvstr = buf;
#  else
	end = Argv_p[Argc_p-1];
	end += strlen(end);
	len = end - Argv_p[0];
	if( i > len ){
		i = len;
	}
	/* zap the arguments */
	if( i > 0 ){
		memset( Argv_p[0], PROCTITLEPAD, len );
		memcpy( Argv_p[0], buf, i );
	}
#  endif
# endif
#endif				/* SETPROCTITLE */
}

#endif
