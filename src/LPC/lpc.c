/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
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
 *   lpd [pr | pr@host]   - PID of LPD server
 *   active [pr |pr@host] - check to see if server accepting connections
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

#include "lp.h"
#include "control.h"
#include "initialize.h"
#include "killchild.h"
#include "sendlpc.h"
#include "getprinter.h"
#include "waitchild.h"
#include "setuid.h"
#include "decodestatus.h"
#include "permission.h"
#include "linksupport.h"
/**** ENDINCLUDE ****/

static char *const _id =
"lpc.c,v 3.15 1998/03/24 02:43:22 papowell Exp";

/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/




char LPC_optstr[] 	/* LPC options */
 = "AD:P:V";

void use_msg(void);

int main(int argc, char *argv[], char *envp[])
{
	char *s;
	char msg[ LINEBUFFER ];
	char orig_msg[ LINEBUFFER ];
	int action;

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Initialize(argc, argv, envp);


	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);


	/* scan the argument list for a 'Debug' value */

	Get_debug_parm( argc, argv, LPC_optstr, debug_vars );

	/* setup configuration */
	Setup_configuration();


	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */

	/* get the printer name */
	Get_printer(0);
	if( RemoteHost == 0 ){
		Diemsg( _("No remote host specified") );
	}

	DEBUG0("lpc: RemotePrinter '%s', RemoteHost '%s'", RemotePrinter, RemoteHost );
	DEBUG0("lpc: Optind '%d', argc '%d'", Optind, argc );
	if(DEBUGL1){
		int i;
		for( i = Optind; i < argc; ++i ){
			logDebug( " [%d] '%s'", i, argv[i] );
		}
	}

	if( Check_for_rg_group( Logname ) ){
		fprintf( stderr, "cannot use printer - not in privileged group\n" );
		Errorcode = 1;
		cleanup(0);
	}

	if( Optind < argc ){
		/* we have a command line */
		action = Get_controlword( argv[Optind] );
		if( action == 0 ){
			use_msg();
		} else {
			Send_lpcrequest( Logname, &argv[Optind], Connect_timeout,
				Send_query_rw_timeout,
				1, action );
		}
	} else while(1){
		int tokencount, i;
#define MAXTOKEN 20
		struct token tokens[ MAXTOKEN ];
		char *lineargs[ MAXTOKEN+1 ];
	
		fprintf( stdout, "lpc>" );
		fflush( stdout );
		if( fgets( msg, sizeof(msg), stdin ) == 0 ) break;
		if( (s = strchr( msg, '\n' )) ) *s = 0;
		DEBUG1("lpc: '%s'", msg );
		safestrncpy( orig_msg, msg );
		tokencount = Crackline( msg, tokens, MAXTOKEN );
		if( tokencount == 0 ) continue;
		if( tokencount >= MAXTOKEN ){
			fprintf( stdout, _("Too many parameters\n") );
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
		} else if( strcasecmp(s,"lpq") == 0 ){
			pid_t pid, result;
			plp_status_t status;
			if( tokencount == 1 && Printer ){
				i = strlen(orig_msg);
				plp_snprintf( orig_msg+i, sizeof(orig_msg)-i, " -P%s", Printer );
			}
			if( (pid = dofork(0)) == 0 ){
				/* we are going to close a security loophole */
				Full_user_perms();
				/* this would now be the same as executing LPQ as user */
				DEBUG0("lpc: doing system(%s)",orig_msg);
				Errorcode = system( orig_msg );
				DEBUG0("lpc: system() returned %d",Errorcode);
				cleanup(0);
			} else if( pid < 0 ) {
				Diemsg( _("fork failed - %s'"), Errormsg(errno) );
			}
			while(1){
				result = plp_waitpid( pid, &status, 0 );
				if( (result == -1 && errno != EINTR) || result == 0 ) break;
			}
			DEBUG0("lpc: system pid %d, exit status %s",
				result, Decode_status( &status ) );
			continue;
		}
		action = Get_controlword( s );
		if( action == 0 ){
			use_msg();
			continue;
		}
		Send_lpcrequest( Logname, lineargs,
			Connect_timeout,
			Send_query_rw_timeout,
			1, action );
	}
	Errorcode = 0;
	cleanup(0);
	return(0);
}
