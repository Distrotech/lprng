/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: waitchild.c
 * PURPOSE: wait for children
 **************************************************************************/
/***************************************************************************
 *
 * Patrick Powell Sun Apr 16 19:42:18 PDT 1995
 *
 ***************************************************************************/

static char *const _id =
"waitchild.c,v 3.10 1997/12/16 15:06:37 papowell Exp";

#include "lp.h"
#include "decodestatus.h"
#include "waitchild.h"
#include "killchild.h"
#include "timeout.h"
/**** ENDINCLUDE ****/

/*
 * Patrick Powell
 *  We will normally want to wait for children explitly.
 *  Most of the time they can die, but we do not care.
 *  However, some times we want them to cause a BREAK condition
 *  on a system call.
 */

static int Gather_body;

plp_signal_t
sigchld_handler (int signo)
{
	DEBUG3 ("sigchld_handler: caught SIGCHLD");
	++Child_exit;
	if( Gather_body ){
		int report;
		plp_status_t status;
		while( (report = plp_waitpid( -1, &status, WNOHANG )) > 0 );
		(void) plp_signal_break(SIGCHLD, sigchld_handler);
	}
	plp_signal(SIGCHLD, SIG_DFL );
}

pid_t plp_waitpid (pid_t pid, plp_status_t *statusPtr, int options)
{
	int report = -1;
	DEBUG2("plp_waitpid: pid %d, options %d", pid, options );
	report = waitpid(pid, statusPtr, options );
	DEBUG2("plp_waitpid: report %d, status %s", report,
		Decode_status( statusPtr ) );
	return report;
}

pid_t plp_waitpid_timeout(int timeout,
	pid_t pid, plp_status_t *status, int options)
{
	int report = -1;
	int err;
	DEBUG2("plp_waitpid_timeout: timeout %d, pid %d, options %d",
		timeout, pid, options );
	if( Set_timeout() ){
		Set_timeout_alarm( timeout, 0 );
		report = plp_waitpid( pid, status, options );
		err = errno;
	} else {
		report = -1;
		err = EINTR;
	}
	Clear_timeout();
	DEBUG2("plp_waitpid_timeout: report %d, status %s", pid,
		Decode_status( status ) );
	errno = err;
	return( report );
}

void Setup_waitpid (void)
{
	signal( SIGCHLD, SIG_DFL );
}

void Setup_waitpid_break (int gather_body )
{
	Gather_body = gather_body;
	(void) plp_signal_break(SIGCHLD, sigchld_handler);
}
