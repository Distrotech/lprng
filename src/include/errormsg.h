/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: errormsg.h,v 5.5 2000/12/25 01:51:17 papowell Exp papowell $
 ***************************************************************************/



#ifndef _ERRORMSG_H_
#define _ERRORMSG_H_ 1

#if defined(HAVE_STDARGS)
void logmsg(int kind, char *msg,...);
void fatal(int kind, char *msg,...);
void logerr(int kind, char *msg,...);
void logerr_die(int kind, char *msg,...);
void Diemsg(char *msg,...);
void Warnmsg(char *msg,...);
void logDebug(char *msg,...);
void Message(char *msg,...);
#else
void logmsg();
void fatal();
void logerr();
void logerr_die();
void Diemsg();
void Warnmsg();
void logDebug();
void Message();
#endif

#if defined(FORMAT_TEST)
#define LOGMSG(X) printf(
#define FATAL(X) printf(
#define LOGERR(X) printf(
#define LOGERR_DIE(X) printf(
#define LOGDEBUG printf
#define DIEMSG printf
#define WARNMSG printf
#define MESSAGE printf
#else
#define LOGMSG(X) logmsg(X,
#define FATAL(X) fatal(X,
#define LOGERR(X) logerr(X,
#define LOGERR_DIE(X) logerr_die(X,
#define LOGDEBUG logDebug
#define DIEMSG Diemsg
#define WARNMSG Warnmsg
#define MESSAGE Message
#endif

/* PROTOTYPES */
const char * Errormsg( int err );
const char *Sigstr(int n);
const char *Decode_status(plp_status_t *status);
char *Server_status( int d );

#endif
