/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: bsd-compat.c
 * PURPOSE: BSD UNIX compatibility routines
 **************************************************************************/

static char *const _id =
"$Id: bsd-compat.c,v 3.3 1997/01/19 14:34:56 papowell Exp $";

/*******************************************************************
 * Some stuff for Solaris and other SVR4 impls; emulate BSD sm'tics.
 * feel free to delete the UCB stuff if we need to tighten up the
 * copyright -- it's just there to help porting.
 *
 * Patrick Powell Thu Apr  6 20:08:56 PDT 1995
 * Taken from the PLP Version 4 Software Distribution
 ******************************************************************/

#include "lp.h"
#include "bsd-compat.h"
/**** ENDINCLUDE ****/

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

/* plp_signal will set flags so that signal handlers will continue
 * note that in Solaris,  you MUST reinstall the
 * signal hanlders in the signal handler!  The default action is
 * to try to restart the system call - note that the code should
 * be written so that you check for error returns, and continue
 * so this is merely a convenience.
 */

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT            /* SunOS */
#endif
		;
	} else {
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART             /* SVR4, 4.3+BSD */
#endif
		;
	}
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
}

/* plp_signal_break is similar to plp_signal,  but will cause
 * TERMINATION of a system call if possible.  This allows
 * you to force a signal to cause termination of a system
 * wait or other action.
 */

plp_sigfunc_t plp_signal_break (int signo, plp_sigfunc_t func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;            /* SunOS */
#endif
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

plp_sigfunc_t plp_signal_break (int signo, plp_sigfunc_t func)
{
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
}
#endif

/**************************************************************/

#ifdef HAVE_SIGPROCMASK

void 
plp_block_all_signals ( plp_block_mask *oblock ) {
	sigset_t block;

	(void) sigfillset (&block); /* block all signals */
	if (sigprocmask (SIG_BLOCK, &block, oblock) < 0)
		logerr_die( LOG_ERR, "plp_block_all_signals: sigprocmask failed");
}

void 
plp_unblock_all_signals ( plp_block_mask *oblock ) {
	(void) sigprocmask (SIG_SETMASK, oblock, (sigset_t *) 0);
}

void 
plp_block_one_signal( int sig, plp_block_mask *oblock ) {
	sigset_t block;

	(void) sigemptyset (&block); /* clear out signals */
	(void) sigaddset (&block, sig ); /* clear out signals */
	if (sigprocmask (SIG_BLOCK, &block, oblock ) < 0)
		logerr_die( LOG_ERR, "plp_block_one_signal: sigprocmask failed");
}

void
plp_sigpause( void ) {
	sigset_t block;
	(void) sigemptyset (&block); /* clear out signals */
	(void) sigsuspend( &block );
}
#else
/* ! HAVE_SIGPROCMASK */

void 
plp_block_all_signals ( plp_block_mask *omask ) {
	*omask = sigblock( ~0 ); /* block all signals */
}

void 
plp_unblock_all_signals ( plp_block_mask *omask ) {
	(void) sigsetmask (*omask);
}

void 
plp_block_one_signal( int sig, plp_block_mask *omask ) {
	*omask = sigblock( sigmask( sig ) );
}

void
plp_sigpause( void ) {
	(void)sigpause( 0 );
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

/***************************************************************************
 * plp_usleep() with select - simple minded way to avoid problems
 ***************************************************************************/
int plp_usleep( int i )
{
	struct timeval t;
	DEBUG3("plp_sleep: starting sleep %d", i );
	if( i > 0 ){
		memset( &t, 0, sizeof(t) );
		t.tv_usec = i;
		i = select( 0,
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			&t );
		DEBUG3("plp_sleep: select done, status %d", i );
	}
	return( i );
}


/***************************************************************************
 * int get_max_processes()
 *  get the maximum number of processes allowed
 ***************************************************************************/

int Get_max_servers( void )
{
#if defined(CHILD_MAX)
	int n = CHILD_MAX;	/* We need some sort of limit here */
#else
	int n = 20;	/* We need some sort of limit here */
#endif

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NPROC)
	struct rlimit pcount;
	if( getrlimit(RLIMIT_NPROC, &pcount) == -1 ){
		fatal( LOG_ERR, "get_max_processes: getrlimit failed" );
	}
	n = pcount.rlim_cur;
#endif
	n = n/2;
	if( Max_servers_active && Max_servers_active < n ){
		n = Max_servers_active;
	}
	return( n );
}
