/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: errormsg.h
 * PURPOSE: identifies error message information, see errormsg.c
 * errormsg.h,v 3.1 1996/12/28 21:40:26 papowell Exp
 **************************************************************************/

#ifndef _ERRORMSG_H
#define _ERRORMSG_H

#if defined(HAVE_STDARGS)
void log (int kind, char *msg,...);
void fatal (int kind, char *msg,...);
void logerr (int kind, char *msg,...);
void logerr_die (int kind, char *msg,...);
void Diemsg (char *msg,...);
void Warnmsg (char *msg,...);
void logDebug (char *msg,...);
#else
void log ();
void fatal ();
void logerr ();
void logerr_die ();
void Diemsg ();
void Warnmsg ();
void logDebug ();
#endif

const char * Errormsg ( int err );

void Malloc_failed( unsigned size );

#endif
