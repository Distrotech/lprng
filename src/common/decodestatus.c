/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: decodestatus.c
 * PURPOSE: Decode dead process error status
 **************************************************************************/

static char *const _id =
"$Id: decodestatus.c,v 3.2 1996/08/31 21:11:58 papowell Exp papowell $";
#include "lp.h"
#include "decodestatus.h"

/***************************************************************************
 * char *Sigstr(n)
 * Return a printable form the the signal
 ***************************************************************************/

#if !defined(HAVE_SYS_SIGLIST) || defined(SOLARIS)
# undef HAVE_SYS_SIGLIST

#define PAIR(X) { #X , X }

static struct signame {
    char *str;
    int value;
} signals[] = {
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

#ifdef HAVE_SYS_SIGLIST
    if (n < NSIG && n >= 0) {
		s = sys_siglist[n];
	}
#else
	int i;

	for( i = 0; signals[i].str && signals[i].value != n; ++i );
	s = signals[i].str;
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

    *msg = 0;		/* just in case! */
    if (WIFEXITED (*status)) {
		(void) plp_snprintf (msg, sizeof(msg),
		"exited with status %d (%s)", WEXITSTATUS (*status),
				 Server_status(*status) );
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
char *Server_status( int d )
{
	char *s;
	static char msg[LINEBUFFER];
	switch( d ){
		case JSUCC: s = "SUCCESS"; break;
		case JFAIL: s = "FAILURE"; break;
		case JABORT: s = "ABORT"; break;
		case JREMOVE: s = "REMOVE"; break;
		case JACTIVE: s = "ACTIVE"; break;
		case JIGNORE: s = "IGNORE"; break;
		default:
			s = msg;
			if( d > 1 && d < 32 ){
				plp_snprintf( msg, sizeof(msg), "TERMINATED SIGNAL %s",
					Sigstr( d ) );
			} else {
				plp_snprintf( msg, sizeof(msg), "UNKNOWN STATUS '%d'", d );
			}
			break;
	}
	return(s);
}
