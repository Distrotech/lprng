/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlpq.c
 * PURPOSE: Send a status request to the remote host
 *
 **************************************************************************/

static char *const _id =
"sendlpq.c,v 3.10 1998/01/08 09:51:18 papowell Exp";

#include "lp.h"
#include "sendlpq.h"
#include "linksupport.h"
#include "sendauth.h"
#include "readstatus.h"
#include "killchild.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/


/***************************************************************************
Commentary:
The protocol used to send a status request to a remote host consists of the
following:

Client                                   Server
- short format
\3printername option option\n - request
                                         status\n
										....
                                         status\n
										<close connection>

- long format
\4printername option option\n - request


The requestor:
1.  makes a connection (connection timeout)
2.  sends the \3printername options line
3.  reads from the connection until nothing left

void Send_lpqrequest( char *printer,	* name of printer *
	char *host,					* name of remote host *
	int format,					* long format == 1, short == 0 *
	char **options,				* options to send *
	int connect_timeout,		* timeout on connection *
	int transfer_timeout,		* timeout on transfer *
	int output )				* output fd

 **************************************************************************/


/***************************************************************************
 * Send_lpqrequest
 * 1. Open connection to remote host
 * 2. Send a line of the form:
 *     - short status  \3Printer <options>
 *     - long status   \4Printer <options>
 * 3. Read from the connection until closed and echo on fd 1
 ***************************************************************************/

/* number of -llll before giving up and showing all */


void Send_lpqrequest( char *printer,	/* name of printer */
	char *host,					/* name of remote host */
	int format,					/* long format == 1, long long = 2, short == 0 */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output )				/* output fd */
{
	char *s;
	int status;
	int sock;		/* socket to use */
	char line[LINEBUFFER];
	char msg[LINEBUFFER];
	int err;

	DEBUG3("Send_lpqrequest: Interactive %d, format %d",
		Interactive, format );
	DEBUG3("Send_lpqrequest: connect_timeout %d, transfer_timeout %d",
			connect_timeout, transfer_timeout );

	if( Remote_support ){
		if( strchr( Remote_support, 'Q' ) == 0 
			&& strchr( Remote_support, 'q' ) == 0 ){
			plp_snprintf( line, sizeof(line)-2,
				_("no remote support to `%s@%s'"), printer, host );
			s = line;
			goto error;
		}
		if( format == REQ_VERBOSE && strchr( Remote_support, 'V' ) == 0 
			&& strchr( Remote_support, 'v' ) == 0 ){
			plp_snprintf( line, sizeof(line)-2,
				_("no verbose status support to `%s@%s'"), printer, host );
			s = line;
			goto error;
		}
	}
	sock = Link_open( host, connect_timeout );
	err = errno;
	if( sock < 0 ){
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection to `%s@%s' - %s",
			printer, host, Errormsg(err) );
		s = line;
		goto error;
	}

	/* we check if we need to do authentication */
	Fix_auth();

	/* now format the option line */
	plp_snprintf( line, sizeof(line), "%c%s\n",
		format, printer );
	s = 0;
	for( ; options && (s = *options); ++options ){
		int len = strlen(line) - 1;
		if( len+strlen(s) > sizeof(line) - 4 ){
			break;
		}
		plp_snprintf( line+len, sizeof(line)-len, " %s\n", s );
	}
	DEBUG3("Send_lpqrequest: Is_server %d, Auth_from %d, Forward_auth %s",
		Is_server, Auth_from, Forward_auth );
	if( s ){
		Write_fd_str( output, "too many options or options too long\n" );
		Link_close( &sock );
	} else if( (Is_server && Auth_from && Forward_auth )
		|| (!Is_server && (Use_auth || Use_auth_flag)) ){
		DEBUG3("Send_lpqrequest: using authentication" );
		status = Send_auth_command( RemotePrinter, host, &sock,
			transfer_timeout, line, output );
	} else {
		DEBUG3("Send_lpqrequest: sending '%s'", line );
		status = Link_send( host, &sock, transfer_timeout,
			line, strlen(line), 0 );
		if( status ){
			s = line;
			goto error;
		}
	}
	Read_status_info( Printer, 0, sock, host, output, transfer_timeout );
	/* report the status */
	Link_close( &sock );
	return;

error:
	plp_snprintf( msg,sizeof(msg), "Host '%s' - %s\n",
		host, s );
	if( Write_fd_str( output, msg ) < 0 ) cleanup(0);
	return;
}
