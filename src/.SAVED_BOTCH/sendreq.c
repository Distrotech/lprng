/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendreq.c,v 1.31 2002/05/06 16:03:45 papowell Exp $";


#include "lp.h"

#include "child.h"
#include "fileopen.h"
#include "getqueue.h"
#include "linksupport.h"
#include "readstatus.h"
#include "user_auth.h"
#include "sendauth.h"
#include "sendreq.h"

/**** ENDINCLUDE ****/


/***************************************************************************
 * Commentary:
 * The protocol used to send requests to a remote host consists of the
 * following:
 * 
 * Client                                   Server
 * - short format
 * \Xprintername option option\n - request
 *                                          status\n
 * 										....
 *                                          status\n
 * 										<close connection>
 * 
 * The requestor:
 * 1.  makes a connection (connection timeout)
 * 2.  sends the \3printername options line
 * 3.  reads from the connection until nothing left
 * 
 * int Send_request( char *printer,	* name of printer *
 * 	char *host,					* name of remote host *
 * 	int class,					* 'Q'= LPQ, 'C'= LPC, M = lprm *
 * 	int format,					* X value for request *
 * 	char **options,				* options to send *
 * 	int connnect_timeout,		* timeout on connection *
 * 	int transfer_timeout,		* timeout on transfer *
 * 	int output )				* output fd
 * 
 **************************************************************************/


/***************************************************************************
 * Send_request
 * 1. Open connection to remote host
 * 2. Send a line of the form:
 *     \Xprinter <options>
 * 3. Read from the connection until closed and echo on fd 1
 ***************************************************************************/

int Send_request(
	int class,					/* 'Q'= LPQ, 'C'= LPC, M = lprm */
	int format,					/* X for option */
	char **options,				/* options to send */
	int connnect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	)
{
	char errormsg[LARGEBUFFER];
	char *cmd = 0;
	int status = -1, sock = -1, err;
	char *real_host = 0;
	struct security *security = 0;
	struct line_list info;

	Init_line_list(&info);
	errormsg[0] = 0;

	DEBUG1("Send_request: printer '%s', host '%s', class '%c', format 0x%02x, "
		"connnect_timeout %d, transfer_timeout %d",
			RemotePrinter_DYN, RemoteHost_DYN, format,
			connnect_timeout, transfer_timeout );

	security = Fix_send_auth(0,&info, 0, errormsg, sizeof(errormsg) );

	if( security ){
		DEBUG1("Send_request: security name '%s', tag '%s'",
			security->name, security->config_tag );
	}
	if( errormsg[0] ){
		goto error;
	}

	if( islower( class ) ) class = toupper(class);

	if( Remote_support_DYN ) uppercase( Remote_support_DYN );
	if( islower(class) ) class = toupper(class);
	if( safestrchr( Remote_support_DYN, class ) == 0 ){
		char *m = "unknown";
		switch( class ){
		case 'R': m = "lpr"; break;
		case 'M': m = "lprm"; break;
		case 'Q': m = "lpq"; break;
		case 'V': m = "lpq -v"; break;
		case 'C': m = "lpc"; break;
		}
		DEBUG1("Send_request: no remote support for %c - '%s' operation", class, m );
		if( !Is_server ){
			SNPRINTF(errormsg,sizeof(errormsg))
			_("no network support for '%s' operation"), m);
		}
		status = 0;
		goto error;
	}

	cmd = safestrdup2(" ",RemotePrinter_DYN,__FILE__,__LINE__);
	cmd[0] = format;
	if( options ){
		for( ; options && *options; ++options ){
			cmd = safeextend3(cmd," ",*options, __FILE__,__LINE__ );
		}
	}
	DEBUG1("Send_request: command '%s'", cmd ); 
	cmd = safeextend2(cmd,"\n", __FILE__,__LINE__ );
	errno = 0;

	sock = Link_open_list( RemoteHost_DYN,
		&real_host, 0, connnect_timeout, 0, Unix_socket_path_DYN );

	err = errno;
	if( sock < 0 ){
		char *msg = "";
		SNPRINTF( errormsg, sizeof(errormsg)-2)
			"cannot open connection - %s",
			err?Errormsg(err):"bad or missing hostname" );
		if( !Is_server ){
			int v = safestrlen(errormsg);
			SNPRINTF( errormsg+v, sizeof(errormsg)-v)
			"\nMake sure the remote host supports the LPD protocol");
			if( geteuid() && getuid() ){
				v = safestrlen(errormsg);
				SNPRINTF( errormsg+v, sizeof(errormsg)-v)
				"\nand accepts connections from this host and from non-privileged (>1023) ports");
			}
		}
		goto error;
	}

	DEBUG1("Send_request: socket %d, real host '%s'", sock, real_host );
	Set_DYN(&RemoteHost_DYN, real_host );
	if( real_host ) free( real_host ); real_host = 0;

	if( security && !(security->flags & SEC_AUTH_SOCKET) ){
		DEBUG1("Send_request: security '%s', using connect", security->name ); 
		status = Send_auth_transfer( &sock, transfer_timeout, 0, 0,
			errormsg, sizeof(errormsg), cmd, security, &info );
	} else {
		status = Link_send( RemoteHost_DYN, &sock, transfer_timeout,
			cmd, safestrlen(cmd), 0 );
	}
	DEBUG3("Send_request: status '%s'", Link_err_str(status) );

 error:
	if( status || errormsg[0] ){
		char line[SMALLBUFFER];
		SNPRINTF( line,sizeof(line)) "Printer '%s@%s' - ",
			RemotePrinter_DYN, RemoteHost_DYN );
		if( Write_fd_str( output, line ) < 0 
			|| Write_fd_str( output, errormsg ) < 0 
			|| Write_fd_str( output, "\n" ) < 0 ) cleanup(0);
	}
	if( cmd ) free(cmd); cmd = 0;
	Free_line_list(&info);
	return( sock );
}
