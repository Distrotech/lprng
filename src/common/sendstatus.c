/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendstatus.c
 * PURPOSE: Send a status request to the remote host
 *
 **************************************************************************/

static char *const _id =
"$Id: sendstatus.c,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lp.h"


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

void Send_statusrequest( char *printer,	* name of printer *
	char *host,					* name of remote host *
	int format,					* long format == 1, short == 0 *
	char **options,				* options to send *
	int connect_timeout,		* timeout on connection *
	int transfer_timeout,		* timeout on transfer *
	int output )				* output fd

 **************************************************************************/


/***************************************************************************
 * Send_statusrequest
 * 1. Open connection to remote host
 * 2. Send a line of the form:
 *     - short status  \3Printer <options>
 *     - long status   \4Printer <options>
 * 3. Read from the connection until closed and echo on fd 1
 ***************************************************************************/

#define MAX_SHORT_STATUS 8
void Send_statusrequest( char *printer,	/* name of printer */
	char *host,					/* name of remote host */
	int format,					/* long format == 1, long long = 2, short == 0 */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output )				/* output fd */
{
	char *s, *end, *colon;
	int i, status;
	int sock;		/* socket to use */
	char line[LINEBUFFER];
	char statusline[256];
	char msg[LINEBUFFER];
	char buffers[MAX_SHORT_STATUS][256];	/* status lines */
	int header_len;
	int cnt, next;
	int sendformat;
	int err;
	int max_count;

	DEBUG4("Send_statusrequest: connect_timeout %d, transfer_timeout %d",
			connect_timeout, transfer_timeout );
	sock = Link_open( host, 0, connect_timeout );
	err = errno;
	if( sock < 0 ){
		plp_snprintf( line, sizeof(line)-2,
			"cannot open connection to `%s@%s' - %s",
			printer, host, Errormsg(err) );
		s = line;
		goto error;
	}
	max_count = 0;
	if( format ){
		max_count = (1 << (format-1));
		if( max_count > MAX_SHORT_STATUS ){
			max_count = 0;
		}
	}

	/* now format the option line */
	line[0] = 0;
	safestrncat( line, printer );
	for( ; options && (s = *options); ++options ){
		safestrncat( line, " " );
		safestrncat( line, s );
	}
	if( strlen( line ) >= sizeof( line ) - 1 ){
		s = "too many options or options too long";
		goto error;
	}
	/* long format = REQ_DLONG, short = REQ_DSHORT */
	sendformat =  (format)?REQ_DLONG:REQ_DSHORT;
	DEBUG4("Send_statusrequest: sending '\\%d'%s'", format, line );
	/* open a connection */
	status = Link_send( host, &sock, transfer_timeout, sendformat, line, '\n', 0 );
	DEBUG4("Send_statusrequest: Interactive %d, format %d, max_count %d",
		Interactive, format, max_count );
	/* report the status */
	if( status == 0 ){
		if( Interactive == 0 || format == 0 || max_count == 0){
			/* short status or all status */
			while( status == 0 ){
				i = 64*1024;
				status = Link_file_read( host, &sock, transfer_timeout,
					transfer_timeout, output, &i, (void *)0 );
			}
		} else {
			/* long status - trim lines */
			DEBUG4("Send_statusrequest: long status" );
			cnt = next = 0;
			header_len = 1;
			s = statusline;
			buffers[0][0] = 0;
			s[0] = 0;
			statusline[sizeof(statusline)-1] = 0;
			while( status == 0 ){
				DEBUG4("Send_statusrequest: left '%s'", statusline );
				s = &statusline[strlen(statusline)];
				i = (sizeof(statusline) -1) - strlen(statusline);
				if( i <= 0 ){
					s = "status line too long!";
					goto error;
				}
				/* read the status line */
				status = Link_read( host, &sock, transfer_timeout, s, &i );
				if( status ) break;
				s[i] = 0;
				DEBUG4("Send_statusrequest: got '%s'", s );
				/* now we have to split line up */
				for( s = statusline; s ; s = end ){
					end = strchr( s, '\n' );
					if( end ){
						*end++ = 0;
					} else {
						/* copy uncompleted line to start of line */
						strncpy( statusline, s, sizeof(statusline) );
						DEBUG4("Send_statusrequest: reached end, left '%s'", s );
						s = 0;
						continue;
					}
					DEBUG4("Send_statusrequest: line found '%s'", s );
					colon = strchr( s, ':' );
					if( colon == 0 || strncmp( s, buffers[0], header_len ) ){
						/* back up to the start */
						DEBUG4("Send_statusrequest: dumping cnt %d, next %d",
							cnt, next );
						next = next - cnt;
						if( next < 0 ) next += MAX_SHORT_STATUS;
						for( i = 0; i < cnt; ++i ){
							if( Link_send( host, &output, transfer_timeout,
								0, buffers[next], '\n', 0 ) ) exit( 0 );
							next = (next+1) % MAX_SHORT_STATUS;
						}
						buffers[0][0] = 0;
						cnt = next = 0;
					}
					if( colon ){
						/* copy to the next currently available line */
						DEBUG4("Send_statusrequest: adding cnt %d, next %d", cnt, next );
						if( cnt == 0 ){
							header_len = (colon - s);
							if( header_len >= sizeof(buffers[0]) ){
								header_len = sizeof(buffers[0]) - 1;
							}
						}
						strncpy( buffers[next], s, sizeof(buffers[0]) - 1 );
						next = (next+1) % MAX_SHORT_STATUS;
						if( cnt < max_count ) ++cnt;
					} else {
						if( Link_send( host, &output, transfer_timeout,
							0, s, '\n', 0 ) ) exit( 0 );
					}
				}
			}
			if( cnt ){
				/* back up to the start */
				DEBUG4("Send_statusrequest: dumping cnt %d, next %d",
					cnt, next );
				next = next - cnt;
				if( next < 0 ) next += MAX_SHORT_STATUS;
				for( i = 0; i < cnt; ++i ){
					if( Link_send( host, &output, 
						transfer_timeout, 0, buffers[next], '\n', 0 ) ) exit(0);
					next = (next+1) % MAX_SHORT_STATUS;
				}
			}
		}
	}
	Link_close( &sock );
	return;

error:
	plp_snprintf( msg,sizeof(msg), "Host '%s' - %s\n",
		host, s );
	if( Write_fd_str( output, msg ) < 0 ){
		cleanup(0);
		exit(-2);
	}
	return;
}
