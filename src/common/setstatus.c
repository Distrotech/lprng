/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
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
static char *const _id = "$Id: setstatus.c,v 3.14 1998/01/08 09:51:19 papowell Exp $";

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
static int setup_logger_fd( char *destination, char **saved_host,
	int logger_fd );

static void set_msg_b( void )
{
	msg_b_len = LARGEBUFFER;
	malloc_or_die( msg_b, msg_b_len+16 );
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

	if( !Is_server ){
		if( Interactive && Verbose ){
			if( msg_b == 0 ) set_msg_b();
			msg_b[0] = 0;
			(void) vplp_snprintf( msg_b, msg_b_len, fmt, ap);
			len = strlen(msg_b);
			strncat( msg_b,"\n", msg_b_len-len );
			if( Write_fd_str( 2, msg_b ) < 0 ) cleanup(0);
		}
		--in_setstatus;
		VA_END;
		return;
	}

	put_header( cfp, "STATUS" );
	len = strlen(msg_b);
	/* save part after header */
	msg = msg_b+len;
	(void) vplp_snprintf( msg_b+len, msg_b_len-len, fmt, ap);

	DEBUG1("setstatus: new status '%s'", msg );
	len = strlen(msg_b);
	at_time = msg_b+len;
	(void) plp_snprintf( msg_b+len, msg_b_len-len, " at %s\n", 
		Time_str(1,0) );

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
		if( save ){
			free( save );
		}
		malloc_or_die( save, minsize+1 );
		save[minsize] = 0;
		
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
		*str = 0;
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
	put_end( cfp );
	--in_setstatus;
	VA_END;
}

/***************************************************************************
 * send_to_logger( struct control_file *cfp, char *msg )
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
	int len;

	if( msg_b == 0 ) set_msg_b();
	msg_b[0] = 0;
	if( cfp ){
		(void) plp_snprintf( msg_b, msg_b_len,
			"IDENTIFIER %s\n", cfp->identifier+1 );
		len = strlen(msg_b);
		plp_snprintf( msg_b+len, msg_b_len-len,
			"JOBNUMBER %d\n", cfp->number );
	}
	if( Printer && *Printer ){
		len = strlen(msg_b);
		plp_snprintf( msg_b+len, msg_b_len-len, "PRINTER %s@%s\n",
			Printer, FQDNHost );
	}
	len = strlen(msg_b);
	(void) plp_snprintf( msg_b+len, msg_b_len-len,
		"AT %s\n", Time_str(0,0));
	if( header && *header ){
		len = strlen(msg_b);
		(void) plp_snprintf( msg_b+len, msg_b_len-len,
			"%s\n", header);
	}
}

static void put_end( struct control_file *cfp )
{
	int len;

	len = strlen(msg_b);
	if( len ){
		msg_b[msg_b_len] = 0;
		if( msg_b[len-1] != '\n' ){
			strcpy( &msg_b[len], "\n.\n" );
		} else {
			strcpy( &msg_b[len], ".\n" );
		}
		send_to_logger( cfp, msg_b );
	}
}

static char *saved_host, *saved_mail;
static int prot_num, port_num;

/***************************************************************************
 * send_to_logger( struct control_file *cfp, char *msg )
 *  This will try and send to the logger.  It usually will not try to
 *  reset a connection unless asked.
 ***************************************************************************/

void send_to_logger( struct control_file *cfp, char *msg )
{
	DEBUG3( "send_to_logger: server %d, dest '%s', olddest '%s',fd %d,msg '%s'",
		Is_server, Logger_destination, saved_host, Logger_fd, msg );
	if( !Is_server  ) return;
	if( msg == 0 ){
		if( Logger_fd > 0 ){
			close( Logger_fd );
			Logger_fd = -1;
		}
		if( Mail_fd > 0 ){
			close( Mail_fd );
			Mail_fd = -1;
		}
		return;
	}
	if( msg[0] == 0 ) return;

	Logger_fd = setup_logger_fd( Logger_destination, &saved_host, Logger_fd );
	if( cfp && Allow_user_logging && hostport( cfp->MAILNAME) ){
		Mail_fd = setup_logger_fd( cfp->MAILNAME+1, &saved_mail, Mail_fd );
	} else if( Mail_fd > 0 ){
		close( Mail_fd );
		Mail_fd = -1;
	}

	if( Logger_fd > 0 ){
		if( Write_fd_str( Logger_fd, msg ) < 0 ){
			DEBUG4("send_to_logger: write to fd %d failed - %s",
				Logger_fd, Errormsg(errno) );
			if( prot_num != SOCK_DGRAM ){
				close( Logger_fd );
				Logger_fd = 0;
			}
		}
	}
	if( Mail_fd > 0 ){
		if( Write_fd_str( Mail_fd, msg ) < 0 ){
			DEBUG4("send_to_logger: write to fd %d failed - %s",
				Mail_fd, Errormsg(errno) );
			if( prot_num != SOCK_DGRAM ){
				close( Mail_fd );
				Mail_fd = 0;
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
	if( Logger_fd > 0 && dup2( Logger_fd, fd ) != -1 ){
		Logger_fd = fd;
	}
}

/*
 * check the logger destination
 *  - if it has changed then close the current one and open
 *    a new one
 */
static int setup_logger_fd( char *destination, char **saved_host,
	int logger_fd )
{
	/* no work to do */
	if( destination && *saved_host && strcmp( destination, *saved_host ) == 0
		&& logger_fd > 0){
		return( logger_fd );
	}
	if( *saved_host ){
		free( *saved_host );
		*saved_host = 0;
	}
	if( logger_fd > 0 ){
		close( logger_fd );
	}
	logger_fd = -1;

	if( destination && *destination ){
		char *port = Default_logger_port;
		char *protocol = Default_logger_protocol;
		char *s;
		struct servent *sp;
		char host[SMALLBUFFER];

		*saved_host = safestrdup( destination );
		safestrncpy( host, destination );

		DEBUG3( "setup_logger_fd: dest '%s',def port'%s',def prot'%s'",
			*saved_host, port, protocol );

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
			fatal( LOG_CRIT, "setup_logger_fd: bad protocol '%s'", protocol );
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
			fatal( LOG_CRIT, "setup_logger_fd: bad port number '%s'", port );
		}
		DEBUG3("setup_logger_fd: host '%s', port %d, protocol %d",
			host, port_num, prot_num );
		logger_fd = Link_open_type(host, 10, port_num, prot_num, 0 );
		DEBUG3("setup_logger_fd: logger_fd '%d'", logger_fd );

		if( logger_fd > 0 && prot_num == SOCK_STREAM ){
			Set_linger(logger_fd, 10);
		}
	}
	return( logger_fd );
}

/*
 * hostport(str)
 *   return 1 if str has format name%port
 */
int hostport( char *str )
{
	int i = 0;
	if( str && !strchr( str, '@' ) 
		&& strchr( str, '%' ) ) i = 1;
	return(i);
}
