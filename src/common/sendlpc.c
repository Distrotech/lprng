/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlpc.c
 * PURPOSE: Send a lpc request to the remote host
 *
 **************************************************************************/

static char *const _id =
"$Id: sendlpc.c,v 3.0 1996/05/19 04:06:10 papowell Exp $";

#include "lp.h"
#include "lp_config.h"
#include "getprinter.h"
#include "control.h"


/***************************************************************************
Commentary:
The protocol used to send a lpc request to a remote host consists of the
following:

\6printer user function options

 ***************************************************************************/

/***************************************************************************
 * Send_lpcrequest
 * 1. Open connection to remote host
 * 2. Send a line of the form:
 *     - short status  \6Printer user command <options>
 * 3. Read from the connection until closed
 ***************************************************************************/

void Send_lpcrequest(
	char *user,					/* user name */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output,					/* output file descriptor */
	int action					/* type of action */
	)
{
	char *s;
	int i, len, status, argc;
	int sock;		/* socket to use */
	char line[LINEBUFFER];
	int err;

	DEBUG4("Send_lpcrequest: connect_timeout %d, transfer_timeout %d",
			connect_timeout, transfer_timeout );

	/*
	 * arguments are of form: 
	 *  action [printer]
	 */

	for( argc = 0; options[argc]; ++argc );
	if( argc == 0 ){
		return;
	}

	if( argc > 1 && action != LPD && action != REREAD ){
		/* second argument is the printer */
		Printer = options[1];
		RemoteHost = 0;
		RemotePrinter = 0;
		/* Find the remote printer and host name */
		Fix_remote_name(0);
		if( RemotePrinter ){
			options[1] = RemotePrinter;
		}
	}

	if( RemoteHost == 0 || *RemoteHost == 0 ){
		RemoteHost = 0;
		if( Default_remote_host && *Default_remote_host ){
			RemoteHost = Default_remote_host;
		} else if( FQDNHost && *FQDNHost ){
			RemoteHost = FQDNHost;
		}
	}
	if( RemoteHost == 0 ){
		plp_snprintf( line, sizeof(line)-2, 
			"no remote host for printer `%s'", Printer );
		goto error;
	}

	DEBUG4("Send_lpcrequest: Printer '%s', Remote printer %s, RemoteHost '%s'",
		Printer, RemotePrinter, RemoteHost );

	sock = Link_open( RemoteHost, 0, connect_timeout );
	err = errno;
	if( sock < 0 ){
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection to `%s@%s' - %s",
			Printer, RemoteHost, Errormsg(err) );
		goto error;
	}

	/* now format the option line */
	plp_snprintf( line, sizeof(line), "%s %s",
		RemotePrinter?RemotePrinter:Printer, user );
	for( i = 0; (s = options[i]); ++i ){
		len = sizeof(line) - (strlen(line) + strlen(s) + 2);
		if( len <= 0 ){
			break;
		}
		strcat( line, " " );
		strcat( line, s );
	}
	/* lets see if they fit */
	if( s ){
		Write_fd_str( 1, "too many options or options too long\n" );
	} else {
		DEBUG4("Send_lpcrequest: sending '\\%d'%s'", REQ_CONTROL, line );
		status = Link_send( RemoteHost, &sock, transfer_timeout, REQ_CONTROL,
			line, '\n', 0 );
		while( status == 0 ){
			i = 64*1024;	/* stop at 64K */
			status = Link_file_read( RemoteHost, &sock, transfer_timeout,
				transfer_timeout, output, &i, (void *)0 );
		}
	}
	Link_close( &sock );
	return;

error:
	setstatus( NORMAL, line );
	if( Interactive ){
		strcat(line, "\n" );
		if( Write_fd_str( 2, line ) < 0 ){
			cleanup( 0 );
			exit(-2);
		}
	}
}
