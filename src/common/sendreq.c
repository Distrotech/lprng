/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendreq.c,v 5.1 1999/09/12 21:32:53 papowell Exp papowell $";


#include "lp.h"
#include "child.h"
#include "fileopen.h"
#include "getqueue.h"
#include "linksupport.h"
#include "readstatus.h"
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
 * 	int connect_tmout,		* timeout on connection *
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
	int connect_tmout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	)
{
	char *errormsg = 0, *tempfile, *cmd = 0;
	int status = -1, sock = -1, err, tempfd;
	char line[SMALLBUFFER];
	char msg[SMALLBUFFER];
	struct sockaddr saddr, *bindto = 0;
	char *real_host = 0;
	char *save_host = 0;

	DEBUG1("Send_request: printer '%s', host '%s', format %d",
		RemotePrinter_DYN, RemoteHost_DYN, format );
	DEBUG1("Send_request: connect_tmout %d, transfer_timeout %d",
			connect_tmout, transfer_timeout );

	bindto = Fix_auth(1, &saddr);

	if( islower( class ) ) class = toupper(class);

	if( Remote_support_DYN ){
		if( safestrchr( Remote_support_DYN, class ) == 0 ){
			DEBUG1("Send_request: no remote support for '%c' operation", class );
			if( !Is_server ){
				errormsg = _("no network support for request");
			} else {
				status = 0;
			}
			goto error;
		}
		if( format == REQ_VERBOSE
			&& safestrpbrk( "vV", Remote_support_DYN ) == 0 ){
			DEBUG1("Send_request: no support for verbose status" );
			if( !Is_server ){
				errormsg = _("no network support for verbose status");
			} else {
				status = 0;
			}
			goto error;
		}
	}

	errno = 0;
	sock = Link_open_list( RemoteHost_DYN, &real_host, 0, connect_tmout, bindto );
	err = errno;
	DEBUG1("Send_request: socket %d, real host '%s'", sock, real_host );

	err = errno;
	if( sock < 0 ){
		msg[0] = 0;
		if( !Is_server && err ){
			plp_snprintf( msg, sizeof(msg),
				"\nMake sure LPD server is running on the server");
		}
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection - %s%s",
			err?Errormsg(err):"bad or missing hostname", msg );
		errormsg = line;
		goto error;
	}
	save_host = safestrdup(RemoteHost_DYN,__FILE__,__LINE__);
	Set_DYN(&RemoteHost_DYN, real_host );
	if( real_host ) free( real_host );

	/* make up the command line */
	line[0] = format;
	line[1] = 0;
	cmd = safestrdup2(line,RemotePrinter_DYN,__FILE__,__LINE__);
	if( options ){
		for( ; options && *options; ++options ){
			cmd = safeextend3(cmd," ",*options, __FILE__,__LINE__ );
		}
	}
	DEBUG1("Send_request: command '%s'", cmd ); 
	cmd = safeextend2(cmd,"\n",__FILE__,__LINE__);

	/* now send the command line */

	if( safestrcasecmp(Auth_DYN, NONEP)
		&& safestrcasecmp(Auth_DYN, KERBEROS4) ){
		/* all but Kerberos 4 */
		tempfd = Make_temp_fd(&tempfile);
		Setup_auth_info( tempfd, cmd );
		close(tempfd);
		DEBUG3("Send_request: using authentication '%s'", Auth_DYN );
		status = Send_auth_transfer( &sock, transfer_timeout, tempfile, 0 );
		if( status ){
			errormsg = "authorized transfer failed";
			close( sock ); sock = -1;
			goto error;
		}
	} else {
		if( !safestrcasecmp(Auth_DYN, KERBEROS4) ){
			/* Kerberos 4 put in header and does preliminary stuff */
			if( Is_server ){
				errormsg = "server to server kerberos 4 not supported";
				close( sock ); sock = -1;
				goto error;
			}
            if( (status = Send_krb4_auth( &sock, 0, transfer_timeout )) ){
				errormsg = "kerberos 4 send authentication failed";
                close( sock ); sock = -1;
                goto error;
			}
		}
		status = Link_send( RemoteHost_DYN, &sock, transfer_timeout,
			cmd, strlen(cmd), 0 );
		if( status ){
			plp_snprintf(line,sizeof(line),"%s",Link_err_str(status));
			errormsg = line;
			close(sock); sock = -1;
			goto error;
		}
	}

 error:
	if( status ){
		plp_snprintf( msg,sizeof(msg), "Printer '%s@%s' - %s\n",
			RemotePrinter_DYN, RemoteHost_DYN, errormsg );
		if( Write_fd_str( output, msg ) < 0 ) cleanup(0);
	}
	if( save_host ){
		Set_DYN(&RemoteHost_DYN,save_host);
		free(save_host); save_host = 0;
	}
	if( cmd ) free(cmd); cmd = 0;
	return( sock );
}
