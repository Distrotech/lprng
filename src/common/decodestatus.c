/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: decodestatus.c
 * PURPOSE: Decode dead process error status
 **************************************************************************/

static char *const _id =
"decodestatus.c,v 3.9 1998/03/29 18:32:47 papowell Exp";
#include "lp.h"
#include "decodestatus.h"
#include "errorcodes.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * char *Sigstr(n)
 * Return a printable form the the signal
 ***************************************************************************/

struct signame {
    char *str;
    int value;
};

#undef PAIR
#ifndef _UNPROTO_
# define PAIR(X) { #X , X }
#else
# define __string(X) "X"
# define PAIR(X) { __string(X) , X }
#endif

#if !defined(HAVE_SYS_SIGLIST)
struct signame signals[] = {
{ "NO SIGNAL", 0 },
#ifdef SIGHUP
PAIR(SIGHUP),
#endif
#ifdef SIGINT
PAIR(SIGINT),
#endif
#ifdef SIGQUIT
PAIR(SIGQUIT),
#endif
#ifdef SIGILL
PAIR(SIGILL),
#endif
#ifdef SIGTRAP
PAIR(SIGTRAP),
#endif
#ifdef SIGIOT
PAIR(SIGIOT),
#endif
#ifdef SIGABRT
PAIR(SIGABRT),
#endif
#ifdef SIGEMT
PAIR(SIGEMT),
#endif
#ifdef SIGFPE
PAIR(SIGFPE),
#endif
#ifdef SIGKILL
PAIR(SIGKILL),
#endif
#ifdef SIGBUS
PAIR(SIGBUS),
#endif
#ifdef SIGSEGV
PAIR(SIGSEGV),
#endif
#ifdef SIGSYS
PAIR(SIGSYS),
#endif
#ifdef SIGPIPE
PAIR(SIGPIPE),
#endif
#ifdef SIGALRM
PAIR(SIGALRM),
#endif
#ifdef SIGTERM
PAIR(SIGTERM),
#endif
#ifdef SIGURG
PAIR(SIGURG),
#endif
#ifdef SIGSTOP
PAIR(SIGSTOP),
#endif
#ifdef SIGTSTP
PAIR(SIGTSTP),
#endif
#ifdef SIGCONT
PAIR(SIGCONT),
#endif
#ifdef SIGCHLD
PAIR(SIGCHLD),
#endif
#ifdef SIGCLD
PAIR(SIGCLD),
#endif
#ifdef SIGTTIN
PAIR(SIGTTIN),
#endif
#ifdef SIGTTOU
PAIR(SIGTTOU),
#endif
#ifdef SIGIO
PAIR(SIGIO),
#endif
#ifdef SIGPOLL
PAIR(SIGPOLL),
#endif
#ifdef SIGXCPU
PAIR(SIGXCPU),
#endif
#ifdef SIGXFSZ
PAIR(SIGXFSZ),
#endif
#ifdef SIGVTALRM
PAIR(SIGVTALRM),
#endif
#ifdef SIGPROF
PAIR(SIGPROF),
#endif
#ifdef SIGWINCH
PAIR(SIGWINCH),
#endif
#ifdef SIGLOST
PAIR(SIGLOST),
#endif
#ifdef SIGUSR1
PAIR(SIGUSR1),
#endif
#ifdef SIGUSR2
PAIR(SIGUSR2),
#endif
{0,0}
    /* that's all */
};

#else /* HAVE_SYS_SIGLIST */
# ifndef HAVE_SYS_SIGLIST_DEF
   extern const char *sys_siglist[];
# endif
#endif

#ifndef NSIG
# define  NSIG 32
#endif

const char *Sigstr (int n)
{
    static char buf[40];
	const char *s = 0;

	if( n == 0 ){
		return( "No signal");
	}
#ifdef HAVE_SYS_SIGLIST
    if (n < NSIG && n >= 0) {
		s = sys_siglist[n];
	}
#else
	{
	int i;
	for( i = 0; signals[i].str && signals[i].value != n; ++i );
	s = signals[i].str;
	}
#endif
	if( s == 0 ){
		s = buf;
		(void) plp_snprintf (buf, sizeof(buf), "signal %d", n);
	}
    return(s);
}

/***************************************************************************
 * Decode_status (plp_status_t *status)
 * returns a printable string encoding return status
 ***************************************************************************/

const char *Decode_status (plp_status_t *status)
{
    static char msg[LINEBUFFER];

	int n;
    *msg = 0;		/* just in case! */
    if (WIFEXITED (*status)) {
		n = WEXITSTATUS(*status);
		if( n > 0 && n < 32 ) n += JFAIL-1;
		(void) plp_snprintf (msg, sizeof(msg),
		"exit status %d (%s)", WEXITSTATUS(*status),
				 Server_status(n) );
    } else if (WIFSTOPPED (*status)) {
		(void) strcpy(msg, "stopped");
    } else {
		(void) plp_snprintf (msg, sizeof(msg), "died%s",
			WCOREDUMP (*status) ? " and dumped core" : "");
		if (WTERMSIG (*status)) {
			(void) plp_snprintf(msg + strlen (msg), sizeof(msg)-strlen(msg),
				 ", %s", Sigstr ((int) WTERMSIG (*status)));
		}
    }
    return (msg);
}

/***************************************************************************
 * char *Server_status( int d )
 *  translate the server status;
 ***************************************************************************/

static struct signame statname[] = {
PAIR(JSUCC),
PAIR(JFAIL),
PAIR(JABORT),
PAIR(JREMOVE),
PAIR(JACTIVE),
PAIR(JIGNORE),
PAIR(JHOLD),
PAIR(JNOSPOOL),
PAIR(JNOPRINT),
PAIR(JSIGNAL),
PAIR(JFAILNORETRY),
{0,0}
};

char *Server_status( int d )
{
	char *s;
	int i;
	static char msg[LINEBUFFER];

	for( i = 0; (s = statname[i].str) && statname[i].value != d; ++i );
	if( s == 0 ){
		s = msg;
		if( d > 0 && d < 32 ){
			plp_snprintf( msg, sizeof(msg), "TERMINATED SIGNAL %s",
				Sigstr( d ) );
		} else {
			plp_snprintf( msg, sizeof(msg), "UNKNOWN STATUS '%d'", d );
		}
	}
	return(s);
}
