/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: errormsg.c
 *
 * error messages and logging
 * log():
 *  --message on stderr and on stdout if "Echo_on_stdout" is set
 *  --if the severity is high enough, will also log using syslog().
 *  --saves the last "non-Debug" log message
 * logerr():
 *  --formats the error message corresponding to "errno" and calls log();
 * fatal():
 *  -- log() and exit() with Errorcode
 * logerr_die:
 *  -- logerr() and exit() with Errorcode
 **************************************************************************/

static char *const _id =
"$Id: errormsg.c,v 3.5 1997/10/15 04:06:28 papowell Exp $";

#include "lp.h"
#include "errormsg.h"
#include "killchild.h"
#include "setstatus.h"
/**** ENDINCLUDE ****/

#if defined(HAVE_SYSLOG_H)
# include <syslog.h>
#endif

/****************************************************************************/


static void use_syslog( int kind, char *msg );

/****************************************************************************
 * char *Errormsg( int err )
 *  returns a printable form of the
 *  errormessage corresponding to the valie of err.
 *  This is the poor man's version of sperror(), not available on all systems
 *  Patrick Powell Tue Apr 11 08:05:05 PDT 1995
 ****************************************************************************/
/****************************************************************************/
#if !defined(HAVE_STRERROR)

# if defined(HAVE_SYS_NERR)
#  if !defined(HAVE_SYS_NERR_DEF)
     extern int sys_nerr;
#  endif
#  define num_errors    (sys_nerr)
# else
#  define num_errors    (-1)            /* always use "errno=%d" */
# endif

# if defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_SYS_ERRLIST_DEF)
     extern const char *const sys_errlist[];
#  endif
# else
#  undef  num_errors
#  define num_errors   (-1)            /* always use "errno=%d" */
# endif

#endif

static int in_log;

const char * Errormsg ( int err )
{
    const char *cp;

#if defined(HAVE_STRERROR)
	cp = strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err < num_errors) {
		cp = sys_errlist[err];
    } else
# endif
	{
		static char msgbuf[32];     /* holds "errno=%d". */
		/* SAFE use of sprintf */
		(void) sprintf (msgbuf, "errno=%d", err);
		cp = msgbuf;
    }
#endif
    return (cp);
}

struct msgkind {
    int var;
    char *str;
};

static struct msgkind msg_name[] = {
    {LOG_CRIT, " (CRIT)"},
    {LOG_ERR, " (ERR)"},
    {LOG_WARNING, " (WARN)"},
    {LOG_NOTICE, " (NOTICE)"},
    {LOG_INFO, " (INFO)"},
    {LOG_DEBUG, ""},
    {0}
};

static char * logmsg (int kind)
{
    int i;
    static char b[32];

	b[0] = 0;
	if( kind < 0 ) return(b);
    for (i = 0; msg_name[i].str; ++i) {
		if ( msg_name[i].var == kind) {
			return (msg_name[i].str);
		}
    }
	/* SAFE USE of plp_snprintf */
    (void) plp_snprintf (b, sizeof(b), "<BAD LOG FLAG %d>", kind);
    return (b);
}

static int init = 0;

static void use_syslog (int kind, char *msg)
{
    /* testing mode indicates that this is not being used
     * in the "real world", so don't get noisy. */

#ifndef HAVE_SYSLOG_H
	/* Note: some people would open "/dev/console", as default
		Bad programmer, BAD!  You should parameterize this
		and set it up as a default value.  This greatly aids
		in testing for portability.
		Patrick Powell Tue Apr 11 08:07:47 PDT 1995
	 */
    if (Syslog_fd > 0
		|| ((Syslog_fd = open( Syslog_device,
				O_WRONLY|O_APPEND|O_NOCTTY, Spool_file_perms )) > 0 ) ){
		int len;

		len = strlen(msg);
		msg[len] = '\n';
		Write_fd_len( Syslog_fd, msg, len+1 );
		msg[len] = 0;
    }

#else                           /* HAVE_SYSLOG_H */
#ifdef HAVE_OPENLOG
	/* use the openlog facility */
    if (init == 0) {
		char *s = "LPRng";
        if (Name && *Name) {
			s = Name;
		}
		openlog (s, LOG_PID | LOG_NOWAIT, SYSLOG_FACILITY);
        init = 1;
    }
    (void) syslog (kind, msg);

#else
    (void) syslog (SYSLOG_FACILITY | kind, msg);
#endif							/* HAVE_OPENLOG */
#endif                          /* HAVE_SYSLOG_H */
}

static char log_buf[SMALLBUFFER];

static void log_backend (int kind)
{
    char stamp_buf[SMALLBUFFER];
	int n;
	char *s;
	plp_block_mask omask;
	int err = errno;

    plp_block_all_signals (&omask); /**/
	/* put the error message into the status file as well */
    /*
     * syslogd does most of the things we do after this point,
	 *	so there's no point in filling up the logs with useless stuff;
	 *  syslog the message here, and then do verbose stuff
     */
    if( kind >= 0 && Is_server ){
        use_syslog (kind, log_buf);
	}
    /*
     * Now build up the state info: timestamp, hostname, argv[0], pid, etc.
     * Don't worry about efficiency here -- it shouldn't happen very often
     * (unless we're debugging).
     */

	stamp_buf[0] = 0;

	if( !Interactive ){
		n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
		(void) plp_snprintf( s, n, "%s", Time_str(0,0) );
		if (ShortHost && *ShortHost ) {
			n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
			(void) plp_snprintf( s, n, " %s", ShortHost );
		}
		if (Name && *Name) {
			n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
			(void) plp_snprintf( s, n, " %s", Name );
		}
		if(DEBUGL0){
			n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
			(void) plp_snprintf(s, n, " [%ld]", (long)getpid ());
			n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
			(void) plp_snprintf(s, n, " %s", logmsg(kind) );
		}
	}

	n = strlen(stamp_buf); s = stamp_buf+n; n = sizeof(stamp_buf)-n;
	(void) plp_snprintf(s, n, " %s\n", log_buf );

    /* use writes here: on HP/UX, using f p rintf really screws up. */
	/* if stdout or stderr is dead, you have big problems! */

    if (Echo_on_fd) {
		Write_fd_str( Echo_on_fd, stamp_buf );
    }
	Write_fd_str( 2, stamp_buf );

    plp_unblock_all_signals ( &omask ); /**/
	errno = err;
}

/*****************************************************
 * Put the printer name at the start of the log buffer
 *****************************************************/
 
static void prefix_printer( void )
{
	log_buf[0] = 0;
    if (Printer && *Printer) {
		plp_snprintf( log_buf, sizeof(log_buf), "%s: ", Printer );
	}
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void log (int kind, char *msg,...)
#else
void log (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int kind;
    char *msg;
#endif
	int n;
	char *s;
	int err = errno;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (kind, int);
    VA_SHIFT (msg, char *);

	if( in_log == 0 ){
		++in_log;
		prefix_printer();
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		(void) vplp_snprintf(s, n, msg, ap);
		log_backend (kind);
		in_log = 0;
	}
    VA_END;
	errno = err;
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void fatal (int kind, char *msg,...)
#else
void fatal (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int kind;
    char *msg;
#endif
	int n;
	char *s;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (kind, int);
    VA_SHIFT (msg, char *);
	
	if( in_log == 0 ){
		++in_log;
		prefix_printer();
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		(void) vplp_snprintf(s, n, msg, ap);
		setstatus( NORMAL, "%s", log_buf );

		log_backend (kind);
		in_log = 0;
	}

    VA_END;
    cleanup(0);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr (int kind, char *msg,...)
#else
void logerr (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int kind;
    char *msg;
#endif
	int err = errno;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (kind, int);
    VA_SHIFT (msg, char *);

	if( in_log == 0 ){
		int n;
		char *s;
		++in_log;
		prefix_printer();
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		(void) vplp_snprintf(s, n, msg, ap);
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		if( err ) (void) plp_snprintf (s, n, " - %s", Errormsg (err));
		setstatus( NORMAL, "%s", log_buf );
		log_backend (kind);
		in_log = 0;
	}
    VA_END;
    errno = err;
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void logerr_die (int kind, char *msg,...)
#else
void logerr_die (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    int kind;
    char *msg;
#endif
	int err = errno;
    VA_LOCAL_DECL

    VA_START (msg);
    VA_SHIFT (kind, int);
    VA_SHIFT (msg, char *);

	if( in_log == 0 ){
		int n;
		char *s;
		++in_log;
		prefix_printer();
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		(void) vplp_snprintf (s, n, msg, ap);
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		if( err ) (void) plp_snprintf (s, n, " - %s", Errormsg (err));
		setstatus( NORMAL, "%s", log_buf );
		log_backend (kind);
		in_log = 0;
	}
    cleanup(0);
    VA_END;
}

/***************************************************************************
 * Diemsg( char *m1, *m2, ...)
 * print error message to stderr, and die
 ***************************************************************************/

/* VARARGS1 */
#ifdef HAVE_STDARGS
void Diemsg (char *msg,...)
#else
void Diemsg (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *msg;
#endif
	char buffer[LINEBUFFER];
	char *s;
	int n;
    VA_LOCAL_DECL

    VA_START (msg);
    VA_SHIFT (msg, char *);
	if( in_log == 0 ){
		++in_log;
		buffer[0] = 0;
		n = strlen(buffer); s = buffer + n; n = sizeof(buffer) - n;
		if (Name && *Name) {
			(void) plp_snprintf(s, n, "%s: ", Name);
		}
		
		n = strlen(buffer); s = buffer + n; n = sizeof(buffer) - n;
		(void) plp_snprintf(s, n, "Fatal error - ");

		n = strlen(buffer); s = buffer + n; n = sizeof(buffer) - n;
		(void) vplp_snprintf (s, n, msg, ap);
		n = strlen(buffer); s = buffer + n; n = sizeof(buffer) - n;
		(void) plp_snprintf (s, n, "\n" );

		/* ignore error, we are dying anyways */
		Write_fd_str( 2, buffer );
		in_log = 0;
	}

    cleanup(0);
    VA_END;
}

/***************************************************************************
 * Warnmsg( char *m1, *m2, ...)
 * print warning message to stderr
 ***************************************************************************/

/* VARARGS1 */
#ifdef HAVE_STDARGS
void Warnmsg (char *msg,...)
#else
void Warnmsg (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *msg;
#endif
	char buffer[LINEBUFFER];
	char *s = buffer;
	int n;
	int err = errno;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (msg, char *);

	if( in_log ) return;
	++in_log;
	buffer[0] = 0;
	n = strlen(buffer); s = buffer+n; n = sizeof(buffer)-n;
    if (Name && *Name) {
        (void) plp_snprintf(s, n, "%s: ", Name);
    }
	n = strlen(buffer); s = buffer+n; n = sizeof(buffer)-n;
    (void) plp_snprintf(s, n, "Warning - ");
	n = strlen(buffer); s = buffer+n; n = sizeof(buffer)-n;
    (void) vplp_snprintf (s, n, msg, ap);
	n = strlen(buffer); s = buffer+n; n = sizeof(buffer)-n;
    (void) plp_snprintf(s, n, "\n");

	Write_fd_str( 2, buffer );
	in_log = 0;
	errno = err;
    VA_END;
}

/* VARARGS1 */
#ifdef HAVE_STDARGS
void logDebug (char *msg,...)
#else
void logDebug (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *msg;
#endif
	int err = errno;
    VA_LOCAL_DECL
    VA_START (msg);
    VA_SHIFT (msg, char *);

	if( in_log == 0 ){
		int n;
		char *s;

		++in_log;
		prefix_printer();
		n = strlen(log_buf); s = log_buf+n; n = sizeof(log_buf)-n;
		(void) vplp_snprintf(s, n, msg, ap);
		log_backend(-1);
		in_log = 0;
	}
	errno = err;
    VA_END;
}

/*
 * Malloc_failed error message
 */

void Malloc_failed( unsigned size )
{
	int err = errno;
	logerr( LOG_ERR, "malloc of %d failed", size );
	errno = err;
	cleanup (0);
}
