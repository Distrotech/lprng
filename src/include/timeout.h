/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: timeout.h
 * PURPOSE: timeout support definitions
 * "$Id: timeout.h,v 3.0 1996/05/19 04:06:35 papowell Exp $"
 **************************************************************************/

#ifndef _TIMEOUT_H 
#define _TIMEOUT_H 

#include <setjmp.h>

EXTERN plp_signal_t (*Old_alarm_fun)(int);      /* save the alarm function */
EXTERN int Alarm_timed_out;                                     /* flag */
EXTERN int *Close_fd;      /* close on timeout */
EXTERN int Timeout_pending;

#if defined(HAVE_SIGSETJMP)
EXTERN sigjmp_buf Timeout_env;
#  define Set_timeout(t,s) (sigsetjmp(Timeout_env,1)==0 && Set_timeout_alarm(t,s))
#else
EXTERN jmp_buf Timeout_env;
#  define Set_timeout(t,s) (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s))
#endif
int Set_timeout_alarm( int timeout, int *socket );
void Clear_timeout();

#endif
