/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
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
static char *const _id = "$Id: setstatus.c,v 3.7 1997/03/05 04:40:04 papowell Exp papowell $";

#include "lp.h"
#include "setstatus.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linksupport.h"
#include "pathname.h"
#include "killchild.h"
/**** ENDINCLUDE ****/

static int in_setstatus;
static char *msg_b;
static int msg_b_len;

static void put_header( struct control_file *cfp, char *header );
static void put_end( struct control_file *cfp );

static void set_msg_b( void )
{
	msg_b_len = LARGEBUFFER;
	malloc_or_die( msg_b, msg_b_len+1 );
}

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
	char *s, *str;
	int len, l;
	struct stat statb;
	static struct dpathname dpath;
	char *path, *at_time = 0;
	char *msg;
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (cfp, struct control_file * );
    VA_SHIFT (fmt, char *);

	if( in_setstatus ) return;
	++in_setstatus;

	put_header( cfp, "STATUS" );
	len = strlen(msg_b);
	msg = msg_b+len;

	(void) vplp_snprintf( msg_b+len, msg_b_len-len, fmt, ap);
	if( Interactive ){
		if( Verbose ){
			len = strlen(msg_b);
			strncat( msg_b,"\n", msg_b_len-len );
			if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
		}
		in_setstatus = 0;
		VA_END;
		return;
	}
	DEBUG3("setstatus: new status '%s'", msg );
	len = strlen(msg_b);
	at_time = msg_b+len;
	(void) plp_snprintf( msg_b+len, msg_b_len-len, " at %s\n", 
		Time_str(-1,0) );

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
		DEBUG4("setstatus: status file '%s'", path);
		if( Status_fd < 0 ){
			logerr_die( LOG_ERR, "setstatus: cannot open '%s'", path );
		}
	}
	path = dpath.pathname;
	/*DEBUG4("setstatus: file '%s', size %d", path, statb.st_size ); */
	str = 0;
	if( size > 0 && statb.st_size > size ){
		/* we truncate it */
		DEBUG4("setstatus: truncating '%s'", path );
		if( save == 0 ){
			malloc_or_die( save, minsize+1 );
		}
		if( lseek( Status_fd, statb.st_size-minsize, SEEK_SET ) < 0 ){
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
			|| Write_fd_str( Status_fd, msg ) < 0 ){
		logerr_die( LOG_ERR, "setstatus: write to status file failed" );
	}
done:
	if( at_time ) *at_time = 0;
	if( Is_server ){
		put_end( cfp );
	}
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
	char pr[LINEBUFFER];

	if( msg_b == 0 ) set_msg_b();
	msg_b[0] = 0;
	pr[0] = 0;
	if( cfp ){
		if( Printer && *Printer ){
			plp_snprintf( pr, sizeof(pr), "PRINTER %s@%s\n",
				Printer, FQDNHost );
		}
		(void) plp_snprintf( msg_b, msg_b_len,
			"IDENTIFIER %s\n%sAT %s\n%s\n",
			cfp->identifier+1, pr, Time_str(-1,0), header );
	}
}

static void put_end( struct control_file *cfp )
{
	int len;

	len = strlen(msg_b);
	if( len && cfp ){
		if( len >= (msg_b_len - 3) ){
			len = msg_b_len - 3;
		}
		if( msg_b[len-1] != '\n' ){
			strcpy( &msg_b[len], "\n.\n" );
		} else {
			strcpy( &msg_b[len], ".\n" );
		}
		send_to_logger( msg_b );
	}
}

static int logger_fd;
static char *saved_host, *host;
static int prot_num, port_num;

void reset_logging( void )
{
	DEBUG3( "reset_logging: logger_fd %d, saved_host '%s'",
		logger_fd, saved_host );
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
}

/***************************************************************************
 * send_to_logger( char *msg )
 *  This will try and send to the logger.  It usually will not try to
 *  reset a connection unless asked.
 ***************************************************************************/

void send_to_logger( char *msg )
{
	DEBUG3( "send_to_logger: dest '%s', olddest '%s',fd %d,msg '%s'",
		Logger_destination, saved_host, logger_fd, msg );
	if( Logger_destination == 0 || *Logger_destination == 0 ){
		return;
	}
	if( msg == 0 || saved_host == 0 || strcmp(saved_host, Logger_destination)){
		char *port = Default_logger_port;
		char *protocol = Default_logger_protocol;
		char *s;
		struct servent *sp;

		reset_logging();
		saved_host = safestrdup( Logger_destination );
		host = safestrdup(saved_host);
		DEBUG3( "send_to_logger: dest '%s',msg '%s',port'%s',prot'%s'",
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
		port_num = 0;
		if( isdigit(port[0]) ){
			/* try integer value */
			port_num = atoi( port );
		} else if( (sp = getservbyname(port, protocol)) ) {
			port_num = ntohs(sp->s_port);
		} else {
			DEBUG3("getservbyname(\"%s\",%s) failed", port, protocol);
		}
		if( port_num <= 0 ){
			Errorcode = JABORT;
			fatal( LOG_CRIT, "send_to_logger: bad port number '%s'", port );
		}
		DEBUG3("send_to_logger: host '%s', port %d, protocol %d",
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
		DEBUG3("send_to_logger: logger_fd '%d'", logger_fd );
	}
	if( msg && logger_fd > 0 ){
		if( Write_fd_str( logger_fd, msg ) < 0 ){
			DEBUG4("send_to_logger: write to fd %d failed - %s",
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
	len = strlen( msg_b );
	(void) vplp_snprintf( msg_b+len, msg_b_len-len, fmt, ap);
	put_end( cfp );
	VA_END;
}


/***************************************************************************
 * Dup_logger_fd( int fd )
 *  Dup the logger_fd file descriptor to fd.
 *  If it fails, don't worry.
 ***************************************************************************/

void Dup_logger_fd( int fd )
{
	if( logger_fd > 0 && dup2( logger_fd, fd ) == 0 ){
		logger_fd = fd;
	}
}
