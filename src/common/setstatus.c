/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setstatus.c
 * PURPOSE: set status in status file or on screen for user
 **************************************************************************/

/***************************************************************************
 * setstatus()
 * - sets dynamic status when required.
 *   This is actually a misdesign - I wanted to use this only for the
 *   LPD server,  but I then discovered that most of the link supprot
 *   routines used setstatus.  Rather than rip it out,  I added the
 *   Interactive and Verbose flags.  It works... what else can I say...
 *
 *	Mon Aug  7 20:49:45 PDT 1995 Patrick Powell
 ***************************************************************************/
static char *const _id = "$Id: setstatus.c,v 3.3 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"

static int in_setstatus;
static char msg[LARGEBUFFER];

static void put_header( struct control_file *cfp, char *header );
static void put_end( struct control_file *cfp );

/*
 * Error status on STDERR
 */
/* VARARGS2 */
#ifdef HAVE_STDARGS
void setstatus (struct control_file *cfp,char *fmt,...)
#else
void setstatus (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct control_file *cfp;
    char *fmt;
#endif
	static char *save;
	static int size, minsize;
	char *s, *str, *startmsg;
	int len, l;
	struct stat statb;
	static struct dpathname dpath;
	char *path, *at_time = 0;
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (cfp, struct control_file * );
    VA_SHIFT (fmt, char *);

	/* prevent recursive calls */
	if( in_setstatus ) return;
	++in_setstatus;
	if( Interactive ){
		if( Verbose ){
			(void) vfprintf ( stderr, fmt, ap);
			(void) fprintf( stderr, "\n" );
		}
		in_setstatus = 0;
		VA_END;
		return;
	}

	put_header( cfp, "STATUS" );
	len = strlen(msg);
	startmsg = msg+len;
	(void) vplp_snprintf( msg+len, sizeof(msg)-len, fmt, ap);
	len = strlen(msg);
	at_time = msg+len;
	(void) plp_snprintf( msg+len, sizeof(msg)-len, " at %s", 
		Time_str(-1,0) );
	DEBUG8("setstatus: new status '%s'", startmsg );
	safestrncat( msg, "\n" );

	/* append new status to end of old */

	if( CDpathname == 0 || CDpathname->pathname[0] == 0
		|| Printer == 0 || *Printer == 0 || fmt == 0 ){
		goto done;
	}

	if( Status_fd == 0 ) Status_fd = -1;
	if( Status_fd > 0 && fstat( Status_fd, &statb ) < 0 ){
		path = dpath.pathname;
		logerr( LOG_ERR, "setstatus: cannot stat '%s'", path );
		close( Status_fd );
		Status_fd = -1;
	}
	if( Status_fd < 0 ){
		size = Max_status_size * 1024;
		if( Min_status_size ){
			minsize = Min_status_size * 1024;
		} else {
			minsize = size / 4;
		}
		if( minsize > size ){
			minsize = size;
		}
		
		dpath = *CDpathname;
		path = Add2_path( &dpath, "status.", Printer );
		Status_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
		DEBUG8("setstatus: status file '%s'", path);
		if( Status_fd < 0 ){
			logerr_die( LOG_ERR, "setstatus: cannot open '%s'", path );
		}
	}
	path = dpath.pathname;
	/*DEBUG8("setstatus: file '%s', size %d", path, statb.st_size ); */
	str = 0;
	if( size > 0 && statb.st_size > size ){
		/* we truncate it */
		DEBUG8("setstatus: truncating '%s'", path );
		if( save == 0 ){
			malloc_or_die( save, minsize+1 );
		}
		if( lseek( Status_fd, (off_t)(statb.st_size-minsize), 0 ) < 0 ){
			logerr_die( LOG_ERR, "setstatus: cannot seek '%s'", path );
		}
		for( len = minsize, str = save;
			len > 0 && (l = read( Status_fd, str, len ) ) > 0;
			str += l, len -= l );
		*str = 0;
		if( (s = strchr( save, '\n' )) ){
			str = s+1;
		} else {
			str = save;
		}
		if( ftruncate( Status_fd, 0 ) < 0 ){
			logerr_die( LOG_ERR, "setstatus: cannot truncate '%s'",
				path );
		}
	}

	if( (str && Write_fd_str( Status_fd, str ) < 0 )
			|| Write_fd_str( Status_fd, startmsg ) < 0 ){
		logerr_die( LOG_ERR, "setstatus: write to status file failed" );
	}
done:
	if( at_time ) *at_time = 0;
	put_end( cfp );
	in_setstatus = 0;
	VA_END;
}

/***************************************************************************
 * send_to_logger( char *msg )
 *  Send a message to the remote logging facility
 *   - note that this routine assumes that the message is in the
 *     required format.
 * Note 1: if you call send_to_logger( 0 ) you will reopen the connection
 * Note 2:
 *  configuration/printcap variables:
 *  
 *  logger_destination =  host[%port][,(TCP|UDP)]
 *  default_logger_port = port       - default logger port
 *  default_logger_protocol = TCP|UDP - default logger protocol
 *  Confi
 ***************************************************************************/

static void put_header( struct control_file *cfp, char *header )
{
	char pr[SMALLBUFFER];

	msg[0] = 0;
	pr[0] = 0;
	if( cfp ){
		if( Printer && *Printer ){
			plp_snprintf( pr, sizeof(pr), " PRINTER %s@%s",
				Printer, FQDNHost );
		}
		(void) plp_snprintf( msg, sizeof(msg),
			"IDENTIFIER %s%s AT %s\n%s\n",
			cfp->identifier, pr, Time_str(-1,0), header );
	}
}

static void put_end( struct control_file *cfp )
{
	if( cfp ){
		safestrncat( msg, ".\n" );
		send_to_logger( msg );
	}
}

static int logger_fd;
static char *saved_host, *host;
static int prot_num, port_num;

void send_to_logger( char *msg )
{

	DEBUG5( "send_to_logger: dest '%s', olddest '%s',fd %d,msg '%s'",
		Logger_destination, saved_host, logger_fd, msg );
	if( Logger_destination == 0 || *Logger_destination == 0 ){
		return;
	}
	if( msg == 0 || saved_host == 0 || strcmp(saved_host, Logger_destination)){
		char *port = Default_logger_port;
		char *protocol = Default_logger_protocol;
		char *s;
		struct servent *sp;
		DEBUG5( "send_to_logger: closing logger_fd %d", logger_fd );
		if( logger_fd > 0 ){
			close( logger_fd );
			logger_fd = 0;
		}
		if( saved_host ){
			free( saved_host );
			saved_host = 0;
		}
		if( host ){
			free( host );
			host = 0;
		}
		saved_host = safestrdup( Logger_destination );
		host = safestrdup(saved_host);
		DEBUG5( "send_to_logger: dest '%s',msg '%s',port'%s',prot'%s'",
			saved_host, msg, port, protocol );
		/* OK, we try to open a connection to the logger */
		if( (s = strchr( host, ',')) ){
			*s = 0;
			protocol = s + 1;
		}
		if( (s = strchr( host, '%')) ){
			*s = 0;
			port = s+1;
		}
		prot_num = SOCK_DGRAM;
		if( protocol == 0 ){
			protocol = "upd";
		} else if( strcasecmp( protocol, "TCP" ) == 0 ){
			protocol = "tcp";
			prot_num = SOCK_STREAM;
		} else if( strcasecmp( protocol, "UDP" ) == 0 ){
			protocol = "udp";
			prot_num = SOCK_DGRAM;
		} else {
			Errorcode = JABORT;
			fatal( LOG_CRIT, "send_to_logger: bad protocol '%s'", protocol );
		}
		if ((sp = getservbyname(port, protocol)) == 0) {
			DEBUG5("getservbyname(\"%s\",%s) failed", port, protocol);
			/* try integer value */
			port_num = atoi( port );
		} else {
			port_num = ntohs(sp->s_port);
		}
		if( port_num <= 0 ){
			Errorcode = JABORT;
			fatal( LOG_CRIT, "send_to_logger: bad port number '%s'", port );
		}
		DEBUG5("send_to_logger: host '%s', port %d, protocol %d",
			host, port_num, prot_num );
	}
	if( logger_fd <= 0 ){
		logger_fd = Link_open_type(host, 0, 10, port_num, prot_num );
		if( logger_fd >= 0 && logger_fd <= 2){
			Errorcode = JABORT;
			fatal( LOG_CRIT,
				"send_to_logger: file descriptor out of range '%d'",
			logger_fd );
		}
		DEBUG5("send_to_logger: logger_fd '%d'", logger_fd );
	}
	if( msg && logger_fd > 0 ){
		if( Write_fd_str( logger_fd, msg ) < 0 ){
			DEBUG9("send_to_logger: write to fd %d failed - %s",
				logger_fd, Errormsg(errno) );
			if( prot_num != SOCK_DGRAM ){
				close( logger_fd );
				logger_fd = 0;
			}
		}
	}
}

/***************************************************************************
 * void setmessage (struct control_file *cfp,char *header, char *fmt,...)
 * put the message out (if necessary) to the logger
 ***************************************************************************/

/* VARARGS2 */
#ifdef HAVE_STDARGS
void setmessage (struct control_file *cfp,char *header, char *fmt,...)
#else
void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct control_file *cfp;
    char *header;
    char *fmt;
#endif
	int len;

    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (cfp, struct control_file * );
    VA_SHIFT (header, char *);
    VA_SHIFT (fmt, char *);

	if( Logger_destination == 0 || *Logger_destination == 0 ){
		return;
	}
	put_header( cfp, header );
	len = strlen( msg );
	(void) vplp_snprintf( msg+len, sizeof(msg)-len, fmt, ap);
	len = strlen( msg );
	if( len > 0 && msg[len-1] != '\n' ){
		safestrncat( msg, "\n" );
	}
	put_end( cfp );
	VA_END;
}
