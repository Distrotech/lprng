/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: timeout.h
 * PURPOSE: timeout support definitions
 * "timeout.h,v 3.2 1997/04/05 23:57:45 papowell Exp"
 **************************************************************************/

#ifndef _TIMEOUT_H 
#define _TIMEOUT_H 

#include <setjmp.h>

EXTERN int Alarm_timed_out;                                     /* flag */
EXTERN int Timeout_pending;
EXTERN int *Close_fd;

#if defined(HAVE_SIGLONGJMP)
EXTERN sigjmp_buf Timeout_env;
#  define Set_timeout() (sigsetjmp(Timeout_env,1)==0)
#else
EXTERN jmp_buf Timeout_env;
#  define Set_timeout() (setjmp(Timeout_env)==0)
#endif
int Set_timeout_alarm( int timeout, int *socket );
void Clear_timeout( void );

#endif
