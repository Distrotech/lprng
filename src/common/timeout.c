/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: timeout.c
 * PURPOSE: generate a timeout indication
 **************************************************************************/

static char *const _id =
"timeout.c,v 3.5 1997/09/18 19:46:07 papowell Exp";

#include "lp.h"
#include "timeout.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * Set up alarms so PLP doesn't hang forever during transfers.
 ***************************************************************************/

/*
 * timeout_alarm
 *  When we get the alarm,  we close the file descriptor (if any)
 *  we are working with.  When we next do an option, it will fail
 *  Note that this will cause any ongoing read/write operation to fail
 * We then to a longjmp to the routine, returning a non-zero value
 * We set an alarm using:
 *
 * if( (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s)) ){
 *   timeout dependent stuff
 * }
 * Clear_alarm
 * We define the Set_timeout macro as:
 *  #define Set_timeout(t,s) (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s))
 */

static plp_signal_t timeout_alarm (int sig)
{
	if( Close_fd && *Close_fd >= 0 ){
		(void) close( *Close_fd );
		*Close_fd = -1;
	}
	Alarm_timed_out = 1;
	signal( SIGALRM, SIG_IGN );
#if defined(HAVE_SIGLONGJMP)
	siglongjmp(Timeout_env,1);
#else
	longjmp(Timeout_env,1);
#endif
}


/***************************************************************************
 * Set_timeout( int timeout, int *socket )
 *  Set up a timeout to occur; note that you can call this
 *   routine several times without problems,  but you must call the
 *   Clear_timeout routine sooner or later to reset the timeout function.
 *  A timeout value of 0 never times out
 * Clear_alarm()
 *  Turns off the timeout alarm
 ***************************************************************************/

int Set_timeout_alarm( int timeout, int *socket )
{
	int err = errno;

	signal(SIGALRM, SIG_IGN);
	alarm(0);
	Alarm_timed_out = 0;
	Close_fd = socket;
	Timeout_pending = 0;

	if( timeout > 0 ){
		Timeout_pending = timeout;
		plp_signal_break(SIGALRM, timeout_alarm);
		alarm (timeout);
	}
	errno = err;
	return(1);
}

void Clear_timeout( void )
{
	int err = errno;

	signal( SIGALRM, SIG_IGN );
	alarm(0);
	Timeout_pending = 0;
	Close_fd = 0;
	errno = err;
}
