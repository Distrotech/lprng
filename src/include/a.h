/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: a.h,v 1.1 2002/02/25 17:43:17 papowell Exp $
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
const char * Errormsg ( int err );
/* VARARGS2 */
#ifdef HAVE_STDARGS
void logmsg(int kind, char *msg,...)
void logmsg(int kind, char *msg,...)
#else
#else
void logmsg(va_alist) va_dcl
void logmsg(va_alist) va_dcl
#endif
#endif;

;
/* VARARGS2 */
#ifdef HAVE_STDARGS
void fatal (int kind, char *msg,...)
void fatal (int kind, char *msg,...)
#else
#else
void fatal (va_alist) va_dcl
void fatal (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr (int kind, char *msg,...)
void logerr (int kind, char *msg,...)
#else
#else
void logerr (va_alist) va_dcl
void logerr (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr_die (int kind, char *msg,...)
void logerr_die (int kind, char *msg,...)
#else
#else
void logerr_die (va_alist) va_dcl
void logerr_die (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS1 */
#ifdef HAVE_STDARGS
void Diemsg (char *msg,...)
void Diemsg (char *msg,...)
#else
#else
void Diemsg (va_alist) va_dcl
void Diemsg (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS1 */
#ifdef HAVE_STDARGS
void Warnmsg (char *msg,...)
void Warnmsg (char *msg,...)
#else
#else
void Warnmsg (va_alist) va_dcl
void Warnmsg (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS1 */
#ifdef HAVE_STDARGS
void Message (char *msg,...)
void Message (char *msg,...)
#else
#else
void Message (va_alist) va_dcl
void Message (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS1 */
#ifdef HAVE_STDARGS
void logDebug (char *msg,...)
void logDebug (char *msg,...)
#else
#else
void logDebug (va_alist) va_dcl
void logDebug (va_alist) va_dcl
#endif
#endif;

;
const char *Sigstr (int n);
const char *Decode_status (plp_status_t *status);
char *Server_status( int d );
/* VARARGS2 */
#ifdef HAVE_STDARGS
void setstatus (struct job *job,char *fmt,...)
void setstatus (struct job *job,char *fmt,...)
#else
#else
void setstatus (va_alist) va_dcl
void setstatus (va_alist) va_dcl
#endif
#endif;

;
/* VARARGS2 */
#ifdef HAVE_STDARGS
void setmessage (struct job *job,const char *header, char *fmt,...)
void setmessage (struct job *job,const char *header, char *fmt,...)
#else
#else
void setmessage (va_alist) va_dcl
void setmessage (va_alist) va_dcl
#endif
#endif;

;

#endif
