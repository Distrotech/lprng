/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: proctitle.h,v 5.1 1999/09/12 21:33:08 papowell Exp papowell $
 ***************************************************************************/



#ifndef _PROCTITLE_H_
#define _PROCTITLE_H_ 1


void initsetproctitle(int argc, char *argv[], char *envp[]);
/* VARARGS3 */
#if !defined(HAVE_SETPROCTITLE) || !defined(HAVE_SETPROCTITLE_DEF)
#  ifdef HAVE_STDARGS
void setproctitle( const char *fmt, ... );
void proctitle( const char *fmt, ... );
#  else
void setproctitle();
void proctitle();
#  endif
#endif

/* PROTOTYPES */

#endif
