/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpc.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lpc [ -PPrinter ] [command]
 * commands:
 *
 *   status [printer]     - show printer status (default is all printers)
 *   start [printer]      - start printing
 *   stop [printer]       - stop printing
 *   enable [printer]     - enable spooling
 *   disable [printer]    - disable spooling
 *   abort [printer]      - stop printing, kill server
 *   kill [printer]       - stop printing, kill server, restart printer
 *
 *   topq printer (user [@host] | host | jobnumer)*
 *   hold printer (all | user [@host] | host |  jobnumer)*
 *   release printer (all | user [@host] | host | jobnumer)*
 *
 *   lprm printer [ user [@host]  | host | jobnumber ] *
 *   lpq printer [ user [@host]  | host | jobnumber ] *
 *   lpd                  - PID of LPD server
 *
 * DESCRIPTION
 *   lpc sends a  request to lpd(8)
 *   and reports the status of the command
 ****************************************************************************
 *
Implementation Notes
Patrick Powell Wed Jun 28 21:28:40 PDT 1995

The LPC program is an extremely simplified front end to the
LPC functionality in the server.  The commands send to the LPD
server have the following format:

\6printer user command options

If no printer is specified, the printer is the default from the
environment variable, etc.

*/

#include "lpc.h"
#include "lp_config.h"
#include "control.h"
#include "setuid.h"
#include "getprinter.h"

static char *const _id =
"$Id: lpc.c,v 3.2 1996/09/09 14:24:41 papowell Exp papowell $";

/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/


void Send_lpcrequest(
	char *user,					/* user name */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output,					/* output file descriptor */
	int action					/* action */
	);

int main(int argc, char *argv[], char *envp[])
{
	char *s;
	char msg[ LINEBUFFER ];
	int action;

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Initialize();


	/* set signal handlers */
	(void) plp_signal (SIGPIPE, (plp_sigfunc_t)SIG_IGN);
	(void) plp_signal (SIGHUP, cleanup);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);


	/* scan the argument list for a 'Debug' value */

	Opterr = 0;
	Get_debug_parm( argc, argv, LPC_optstr, debug_vars );

	/* Get configuration file information */
	Parsebuffer( "default configuration", Default_configuration,
		lpc_config, &Config_buffers );

	/* get the configuration file information if there is any */
    if( Allow_getenv ){
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name );
		}
		if( (s = getenv( "LPD_CONF" )) ){
			Client_config_file = s;
		}
    }

	DEBUG0("main: Configuration file '%s'",
		Client_config_file?Client_config_file:"NULL" );

	Getconfig( Client_config_file, lpc_config, &Config_buffers );

	if( Debug > 5 ) dump_config_list( "LPC Configuration", lpc_config );


	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

	Get_local_host();

	/* expand the information in the configuration file */
	Expandconfig( lpc_config, &Config_buffers );

	if( Debug > 4 ) dump_config_list( "LPC Configuration After Expansion",
		lpc_config );

	Logname = Get_user_information();

	/* scan the input arguments, setting up values */
	Opterr = 1;
	Get_parms(argc, argv);      /* scan input args */

	/* get the printer name */
	Get_printer();
    if( (RemoteHost == 0 || *RemoteHost == 0) ){
		RemoteHost = 0;
        if( Default_remote_host && *Default_remote_host ){
            RemoteHost = Default_remote_host;
        } else if( FQDNHost && *FQDNHost ){
            RemoteHost = FQDNHost;
        }
    }
	if( RemoteHost == 0 ){
		Diemsg( "No remote host specified" );
	}
    if( (RemotePrinter == 0 || *RemotePrinter == 0) ){
		RemotePrinter = Printer;
	}

	DEBUG4("lpc: printer '%s', remote host '%s'", Printer, RemoteHost );
	DEBUG4("lpc: Optind '%d', argc '%d'", Optind, argc );
	if( Optind < argc ){
		/* we have a command line */
		Send_lpcrequest( Logname, &argv[Optind],
			Connect_timeout, Send_timeout, 1, 0 );
	} else while(1){
		int tokencount, i;
#define MAXTOKEN 20
		struct token tokens[ MAXTOKEN ];
		char *lineargs[ MAXTOKEN+1 ];
	
		fprintf( stdout, "lpc>" );
		fflush( stdout );
		if( fgets( msg, sizeof(msg), stdin ) == 0 ) break;
		if( (s = strchr( msg, '\n' )) ) *s = 0;
		DEBUG4("lpc: '%s'", msg );
		tokencount = Crackline( msg, tokens, MAXTOKEN );
		if( tokencount == 0 ) continue;
		if( tokencount >= MAXTOKEN ){
			fprintf( stdout, "Too many parameters\n" );
			continue;
		}
		for( i = 0; i < tokencount;  ++i ){
			lineargs[i] = tokens[i].start;
			tokens[i].start[tokens[i].length] = 0;
		}
		lineargs[i] = 0;
		s = lineargs[0];
		if( strcasecmp(s,"exit") == 0 || s[0] == 'q' || s[0] == 'Q' ){
			break;
		}
		action = Get_controlword( s );
		if( action == 0 ){
			use_msg();
			continue;
		}
		Send_lpcrequest( Logname, lineargs,
			Connect_timeout, Send_timeout, 1, action );
	}
	exit(0);
}
