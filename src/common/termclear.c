/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: termclear.c,v 5.2 1999/10/09 20:49:03 papowell Exp papowell $";


#include "lp.h"
#include "termclear.h"
/**** ENDINCLUDE ****/

#if defined(SOLARIS) || defined(HAVE_TERMCAP_H) || defined(HAVE_CURSES_H) || defined(HAVE_TERM_H)

#if defined(SOLARIS)
# if SOLARIS == 270
#   define	_WIDEC_H
#   define	_WCHAR_H
# endif
# include <curses.h>
#else

# if defined(IS_LINUX)
#   include <termios.h>
# endif

# ifdef HAVE_NCURSES_H
#   include <ncurses.h>
# else
#   if defined(HAVE_CURSES_H)
#     include <curses.h>
#   endif
# endif
   /* terminfo has a termcap emulation */
# if defined(HAVE_TERMCAP_H) && !defined(IS_LINUX)
#   include <termcap.h>
# endif
#endif

#if defined(HAVE_TERM_H)
# include <term.h>
#endif


/*
 * Patrick Powell Fri Aug 11 23:04:11 PDT 1995
 *  The following obnoxious piece of code is why
 *  portablilty is sooooo hard to do.
 * 
 * SUNOS: 4.1.1-4.1.3 - termcap(1) man page
 *	  tputs(cp, affcnt, outc)
 *	  register char *cp;
 *	  int affcnt;
 *	  int (*outc)();
 * 
 * BSD-4.4 + - man page
 *	  tputs(register char *cp, int affcnt, int (*outc)());
 * 
 * BSD-4.4 /usr/include/curses.h
 * 	int  tputs (char *, int, void (*)(int));
 * 
 * Solaris 2.4 term.h
 * extern  int  tputs(char *, int, int (*)(char))
 *
 * HP-UX 10.20
 * extern  int     tputs(char *, int, void (*)(int));
 *
 *  Depending on you stdio package, putchar can return (void)
 *  or be a macro, returning wacko values.  We take the easy road,
 *  and return void(), be compatible with definitions.
 */

#define PPUTS_RETTYPE int
#define TPUTS_RETTYPE int
#define PPUTS_VALTYPE int
#define PPUTS_RETVAL(X) (X)
#if defined(IS_BSDI) || defined(HPUX)
#undef PPUTS_RETTYPE
#undef PPUTS_RETVAL
#define PPUTS_RETTYPE void
#define PPUTS_RETVAL(X)
#endif
#if defined(SOLARIS)
#undef PPUTS_VALTYPE
#define PPUTS_VALTYPE char
#endif

#if !defined(HAVE_TGETSTR_DEF) && !defined(IS_LINUX)
char *tgetstr(char *id, char **area);
#endif
#if !defined(HAVE_TGETENT_DEF) && !defined(IS_LINUX)
int tgetent(char *buf, char *name);
#endif

 static PPUTS_RETTYPE pputs(PPUTS_VALTYPE c)
{
	putchar(c); /* this does the work */
	return PPUTS_RETVAL(c);
}

 static int tinit_done;
 static char *TE_string;
 static char *CL_string;
 static char *area, *sp, *ti;
#define MS 1024
 static char bp[MS], strs[MS];

void Term_clear(void)
{
	DEBUG1("Term_clear: start");
	if (tinit_done == 0) {
		tinit_done = 1;
		if( isatty (0) && (sp = getenv ("TERM")) ){
			DEBUG1("Term_clear: TERM '%s'", sp);
			if (tgetent (bp, sp) > 0){
				DEBUG1("Term_clear: tgetent");
				area = strs;
				TE_string = tgetstr ("te", &area);
				if( TE_string ) TE_string = safestrdup(TE_string,__FILE__,__LINE__);
				area = strs;
				CL_string = tgetstr ("cl", &area);
				if( CL_string ) CL_string = safestrdup(CL_string,__FILE__,__LINE__);
				/* check for nasty things */
				area = strs;
				ti=tgetstr("ti", &area);
				if(ti)tputs(ti, 0, pputs);
			} else {
				DEBUG1("Term_clear: tgetent failed");
			}
		}
	}
	DEBUG1("Term_clear: done");
	if (CL_string) tputs (CL_string, 0, pputs);
	fflush(stdout);
}

void Term_finish(void)
{
	if (TE_string) tputs (TE_string, 0, pputs);
	if( TE_string ) free(TE_string); TE_string = 0;
	if( CL_string ) free(CL_string); CL_string = 0;
	fflush(stdout);
}

#else

void Term_clear(void)
{
	DEBUG1("Term_clear: dummy");
}
void Term_finish(void) {;}

#endif
