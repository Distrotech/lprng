/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: bsd-compat.c
 * PURPOSE: BSD UNIX compatibility routines
 **************************************************************************/

static char *const _id =
"$Id: bsd-compat.c,v 3.1 1996/06/30 17:12:44 papowell Exp $";

/*******************************************************************
 * Some stuff for Solaris and other SVR4 impls; emulate BSD sm'tics.
 * feel free to delete the UCB stuff if we need to tighten up the
 * copyright -- it's just there to help porting.
 *
 * Patrick Powell Thu Apr  6 20:08:56 PDT 1995
 * Taken from the PLP Version 4 Software Distribution
 ******************************************************************/

#include "lp.h"

/**************************************************************
 * 
 * signal handling:
 * SIGALRM should be the only signal that terminates system calls;
 * all other signals should NOT terminate them.
 * This signal() emulation function attepts to do just that.
 * (Derived from Advanced Programming in the UNIX Environment, Stevens, 1992)
 *
 **************************************************************/

#ifdef HAVE_SIGACTION

/* solaris 2.3 note: don't compile this with "gcc -ansi -pedantic";
 * due to a bug in the header file, struct sigaction doesn't
 * get declared. :(
 */

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;           /* SunOS */
#endif
	} else {
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;             /* SVR4, 4.3+BSD */
#endif
	}
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
}

#else /* HAVE_SIGACTION */

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func)
{
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
}
#endif

/**************************************************************/

#ifdef HAVE_SIGPROCMASK
static sigset_t oblock;

void 
plp_block_signals () {
	sigset_t block;

	(void) sigemptyset (&block);
	(void) sigaddset (&block, SIGCHLD);
	(void) sigaddset (&block, SIGHUP);
	(void) sigaddset (&block, SIGINT);
	(void) sigaddset (&block, SIGQUIT);
	(void) sigaddset (&block, SIGTERM);
	if (sigprocmask (SIG_BLOCK, &block, &oblock) < 0)
		logerr_die( LOG_ERR, "plp_block_signals: sigprocmask failed");
}

void 
plp_unblock_signals () {
	(void) sigprocmask (SIG_SETMASK, &oblock, (sigset_t *) 0);
}

#else /* HAVE_SIGPROCMASK */

static int omask;

void 
plp_block_signals () {
	omask = sigblock (sigmask (SIGCHLD) | sigmask (SIGHUP)
	  | sigmask (SIGINT) | sigmask (SIGQUIT) | sigmask (SIGTERM));
}

void 
plp_unblock_signals () {
	(void) sigsetmask (omask);
}
#endif

#ifndef HAVE_STRCASECMP
/**************************************************************
 * Bombproof versions of strcasecmp() and strncasecmp();
 **************************************************************/

/* case insensitive compare for OS without it */
int strcasecmp (const char *s1, const char *s2)
{
	int c1, c2, d;
	for (;;) {
		c1 = *s1++;
		c2 = *s2++;
		if (isalpha (c1) && isalpha (c2)) {
			c1 = tolower (c1);
			c2 = tolower (c2);
		}
		if( (d = (c1 - c2 )) || c1 == 0 ) return(d);
	}
	return( 0 );
}
#endif

#ifndef HAVE_STRNCASECMP
/* case insensitive compare for OS without it */
int strncasecmp (const char *s1, const char *s2, int len )
{
	int c1, c2, d;
	for (;len>0;--len){
		c1 = *s1++;
		c2 = *s2++;
		if (isalpha (c1) && isalpha (c2)) {
			c1 = tolower (c1);
			c2 = tolower (c2);
		}
		if( (d = (c1 - c2 )) || c1 == 0 ) return(d);
	}
	return( 0 );
}
#endif

/*
 * duplicate a string safely, generate an error message
 */

char *safestrdup (const char *p)
{
	char *new;

	malloc_or_die( new, strlen (p) + 1 );
	return strcpy( new, p );
}
/*
 * duplicate a string safely, generate an error message
 * add some extra character space to allow for extensions
 */

char *safexstrdup (const char *p, int extra )
{
	char *new;

	malloc_or_die( new, strlen (p) + 1 + extra );
	return strcpy( new, p );
}

/* perform safe comparison, even with null pointers */

int safestrcmp( const char *s1, const char *s2 )
{
	if( (s1 == s2) ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	return( (strcmp)(s1, s2) );
}

#ifndef HAVE_STRTOUL
/* 
 *      Source code for the "strtoul" library procedure.
 * taken from TCL 7.1 by John Ousterhout (ouster@cs.berkeley.edu).
 *
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <ctype.h>

/*
 * The table below is used to convert from ASCII digits to a
 * numerical equivalent.  It maps from '0' through 'z' to integers
 * (100 for non-digit characters).
 */

static char cvtIn[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,               /* '0' - '9' */
	100, 100, 100, 100, 100, 100, 100,          /* punctuation */
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,     /* 'A' - 'Z' */
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35,
	100, 100, 100, 100, 100, 100,               /* punctuation */
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,     /* 'a' - 'z' */
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35};
/*
 *----------------------------------------------------------------------
 *
 * strtoul --
 *
 *      Convert an ASCII string into an integer.
 *
 * Results:
 *      The return value is the integer equivalent of string.  If endPtr
 *      is non-NULL, then *endPtr is filled in with the character
 *      after the last one that was part of the integer.  If string
 *      doesn't contain a valid integer value, then zero is returned
 *      and *endPtr is set to string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

unsigned long int
strtoul(const char *string, const char **endPtr, int base)
	/* char *string;             * String of ASCII digits, possibly
			                     * preceded by white space.  For bases
			                     * greater than 10, either lower- or
			                     * upper-case digits may be used.
			                     */
	/* char **endPtr;            * Where to store address of terminating
			                     * character, or NULL. */
	/* int base;                 * Base for conversion.  Must be less
			                     * than 37.  If 0, then the base is chosen
			                     * from the leading characters of string:
			                     * "0x" means hex, "0" means octal, anything
			                     * else means decimal.
			                     */
{
	register const char *p;
	register unsigned long int result = 0;
	register unsigned digit;
	int anyDigits = 0;

	/*
	 * Skip any leading blanks.
	 */

	p = string;
	while (isspace(*p)) {
		p += 1;
	}

	/*
	 * If no base was provided, pick one from the leading characters
	 * of the string.
	 */
	
	if (base == 0)
	{
		if (*p == '0') {
			p += 1;
			if (*p == 'x') {
			    p += 1;
			    base = 16;
			} else {

			    /*
			     * Must set anyDigits here, otherwise "0" produces a
			     * "no digits" error.
			     */

			    anyDigits = 1;
			    base = 8;
			}
		}
		else base = 10;
	} else if (base == 16) {

		/*
		 * Skip a leading "0x" from hex numbers.
		 */

		if ((p[0] == '0') && (p[1] == 'x')) {
			p += 2;
		}
	}

	/*
	 * Sorry this code is so messy, but speed seems important.  Do
	 * different things for base 8, 10, 16, and other.
	 */

	if (base == 8) {
		for ( ; ; p += 1) {
			digit = *p - '0';
			if (digit > 7) {
			    break;
			}
			result = (result << 3) + digit;
			anyDigits = 1;
		}
	} else if (base == 10) {
		for ( ; ; p += 1) {
			digit = *p - '0';
			if (digit > 9) {
			    break;
			}
			result = (10*result) + digit;
			anyDigits = 1;
		}
	} else if (base == 16) {
		for ( ; ; p += 1) {
			digit = *p - '0';
			if (digit > ('z' - '0')) {
			    break;
			}
			digit = cvtIn[digit];
			if (digit > 15) {
			    break;
			}
			result = (result << 4) + digit;
			anyDigits = 1;
		}
	} else {
		for ( ; ; p += 1) {
			digit = *p - '0';
			if (digit > ('z' - '0')) {
			    break;
			}
			digit = cvtIn[digit];
			if (digit >= base) {
			    break;
			}
			result = result*base + digit;
			anyDigits = 1;
		}
	}

	/*
	 * See if there were any digits at all.
	 */

	if (!anyDigits) {
		p = string;
	}

	if (endPtr != 0) {
		*endPtr = p;
	}

	return result;
}

#endif /* !HAVE_STRTOUL */
