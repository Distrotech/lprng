/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: waitchild.h
 * PURPOSE: waitchild.c functions
 * waitchild.h,v 3.4 1997/12/16 15:06:50 papowell Exp
 **************************************************************************/

#ifndef _WAITCHILD_H
#define _WAITCHILD_H

/*
 *----------------------------------------------------------------------
 *
 * waitpid --
 *
 *      This procedure emulates the functionality of the POSIX
 *      waitpid kernel call, using the BSD wait3 kernel call.
 *      Note:  it doesn't emulate absolutely all of the waitpid
 *      functionality, in that it doesn't support pid's of 0
 *      or < -1.
 *
 * Results:
 *      -1 is returned if there is an error in the wait kernel call.
 *      Otherwise the pid of an exited or suspended process is
 *      returned and *statusPtr is set to the status value of the
 *      process.
 *
 * Side effects:
 *      None.
 * NOTE: you must call Setup_waitpid() first to enable the
 * signal handling mechanism
 *
 *----------------------------------------------------------------------
 */

pid_t plp_waitpid(pid_t pid, plp_status_t *statusPtr, int options);
void Setup_waitpid(void);

pid_t plp_waitpid_timeout(int timeout,
	pid_t pid, plp_status_t *status, int options);

void Setup_waitpid (void);
void Setup_waitpid_break (int gather_body );

EXTERN int Child_exit;
#endif
