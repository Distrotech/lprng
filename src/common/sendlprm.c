/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlprm.c
 * PURPOSE: Send a remove job request to the remote host
 *
 **************************************************************************/

static char *const _id =
"$Id: sendlprm.c,v 3.0 1996/05/19 04:06:10 papowell Exp $";

#include "lp.h"


/***************************************************************************
Commentary:
This code is similar to the code for sending a status request
The protocol used to send a remove job request to a remote host consists of the
following:

Client                                   Server
- short format
\5printername username option option\n - request
                                         status\n
										....
                                         status\n
										<close connection>

The requestor:
1.  makes a connection (connection timeout)
2.  sends the \3printername options line
3.  reads from the connection until nothing left

void Send_lprmrequest( char *printer,	* name of printer *
	char *host,					* name of remote host *
	int format,					* long format == 1, short == 0 *
	char **options,				* options to send *
	int connect_timeout,		* timeout on connection *
	int transfer_timeout,		* timeout on transfer *
	int output )				* output fd

 **************************************************************************/


/***************************************************************************
 * Send_lprmrequest
 * 1. Open connection to remote host
 * 2. Send a line of the form:
 *     - short status  \3Printer <options>
 *     - long status   \4Printer <options>
 * 3. Read from the connection until closed
 ***************************************************************************/

void Send_lprmrequest( char *printer,	/* name of printer */
	char *host,					/* name of remote host */
	char *user,					/* user name */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output )				/* output fd */
{
	char *s;
	int i, status;
	int sock;		/* socket to use */
	char line[LINEBUFFER];
	int err;

	DEBUG4("Send_lprmrequest: connect_timeout %d, transfer_timeout %d",
			connect_timeout, transfer_timeout );
	sock = Link_open( host, 0, connect_timeout );
	err = errno;
	if( sock < 0 ){
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection to `%s@%s' - %s",
			Printer, RemoteHost, Errormsg(err) );
		setstatus( NORMAL, line );
		if( Interactive ){
			strcat(line, "\n" );
			if( Write_fd_str( 2, line ) < 0 ){
				cleanup(0);
				exit(-2);
			}
		}
	}
	DEBUG4("Send_lprmrequest: socket %d", sock );

	/* now format the option line */
	line[0] = 0;
	plp_snprintf(line, sizeof(line), "%s %s", printer, user );
	for( ; options && (s = *options); ++options ){
		safestrncat( line, " " );
		safestrncat( line, s );
	}
	if( strlen( line ) >= sizeof( line ) - 1 ){
		s = "too many options or options too long";
		if( Interactive ){
			Diemsg( s );
		} else {
			log( LOG_INFO, "Send_lprmrequest: %s", s );
			return;
		}
	}
	/* send the REQ_REMOVE request */
	DEBUG4("Send_lprmrequest: sending '\\%d'%s'", REQ_REMOVE, line );
	status = Link_send( host, &sock, transfer_timeout, REQ_REMOVE,
		line, '\n', 0 );
	while( status == 0 ){
		i = 64*1024;
		status = Link_file_read( host, &sock, transfer_timeout,
			transfer_timeout, output, &i, (void *)0 );
	}
	Link_close( &sock );
}
