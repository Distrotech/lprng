/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlprm.c
 * PURPOSE: Send a remove job request to the remote host
 *
 **************************************************************************/

static char *const _id =
"sendlprm.c,v 3.9 1998/01/08 09:51:18 papowell Exp";

#include "lp.h"
#include "sendlprm.h"
#include "killchild.h"
#include "linksupport.h"
#include "readstatus.h"
#include "sendauth.h"
#include "setstatus.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/


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
	int status;
	int sock;		/* socket to use */
	char line[LINEBUFFER];
	int err;

	DEBUG3("Send_lprmrequest: connect_timeout %d, transfer_timeout %d",
			connect_timeout, transfer_timeout );
	if( Remote_support
		&& strchr( Remote_support, 'M' ) == 0 
		&& strchr( Remote_support, 'm' ) == 0 ){
		plp_snprintf( line, sizeof(line)-2,
			_("no remote support for `%s@%s'"), printer, host );
		setstatus( NORMAL, line );
		if( Interactive ){
			strcat(line, "\n" );
			if( Write_fd_str( 2, line ) < 0 ) cleanup(0);
		}
		return;
	}
	sock = Link_open( host, connect_timeout );
	err = errno;
	if( sock < 0 ){
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection to `%s@%s' - %s",
			Printer, host, Errormsg(err) );
		setstatus( NORMAL, line );
		if( Interactive ){
			strcat(line, "\n" );
			if( Write_fd_str( 2, line ) < 0 ) cleanup(0);
		}
		return;
	}
	DEBUG3("Send_lprmrequest: socket %d", sock );

	/* we check if we need to do authentication */
	Fix_auth();

	/* now format the option line */
	line[0] = 0;
	plp_snprintf(line, sizeof(line), "%c%s %s\n",
		REQ_REMOVE, printer, user );
	s = 0;
	for( ; options && (s = *options); ++options ){
		int len = strlen(line) - 1;
		if( len + strlen(s) >= sizeof(line) - 4){
			break;
		}
		plp_snprintf( line+len, sizeof(line)-len,
			" %s\n", s );
	}
	/* send the REQ_REMOVE request */
	if( s ){
		Write_fd_str( output, "too many options or options too long\n" );
		Link_close( &sock );
	} else if( Use_auth || Use_auth_flag ){
		DEBUG3("Send_lprmrequest: using authentication" );
		status = Send_auth_command( RemotePrinter, host, &sock,
			transfer_timeout, line, output );
	} else {
		DEBUG3("Send_lprmrequest: sending '%s'", line );
		status = Link_send( host, &sock, transfer_timeout,
			line, strlen(line), 0 );
	}
	Read_status_info( Printer, 0, sock, host, output, transfer_timeout );
	Link_close( &sock );
}
