/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *    lprm [-a] [-Pprinter ][-Ddebugopt][job#][user]
 * DESCRIPTION
 *   lprm sends a remove job request to lpd(8) server.
 *   The LPD will remove the requested job.
 *    -P printer
 *         Specifies a particular printer, otherwise  the  default
 *         line printer is used (or the value of the PRINTER vari-
 *         able in the environment).  If PRINTER is  not  defined,
 *         then  the  first  entry in the /etc/printcap(5) file is
 *         reported.  Multiple printers can be displayed by speci-
 *         fying more than one -P option.
 *    -a all printers
 ****************************************************************************
 *
Implementation Notes

Patrick Powell Tue Jun 20 10:09:07 PDT 1995
This is basically the same code as LPRM,  differing only in the
information display and parameters.

*/

#include "lprm.h"
#include "printcap.h"
#include "setuid.h"
#include "lp_config.h"
#include "getprinter.h"

static char *const _id =
"$Id: lprm.c,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $";

/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/

extern void usage();

int main(int argc, char *argv[], char *envp[])
{
	char *s;

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Initialize();


	/* set signal handlers */
	(void) plp_signal (SIGPIPE, cleanup);
	(void) plp_signal (SIGHUP, cleanup);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);


	/* scan the argument list for a 'Debug' value */

	Opterr = 0;
	Get_debug_parm( argc, argv, LPRM_optstr, debug_vars );
	Opterr = 1;

	/* Get configuration file information */
	Parsebuffer( "default configuration", Default_configuration,
		lprm_config, &Config_buffers );
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


	DEBUG0("main: Configuration file '%s'", Client_config_file?Client_config_file:"NULL" );

	Getconfig( Client_config_file, lprm_config, &Config_buffers );

	if( Debug > 5 ) dump_config_list( "LPRM Configuration", lprm_config );


	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

	Get_local_host();

	/* expand the information in the configuration file */
	Expandconfig( lprm_config, &Config_buffers );

	if( Debug > 4 ) dump_config_list( "LPRM Configuration After Expansion",
		lprm_config );

	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */
	/* need at least one argument */
	/*if( argc  - Optind <= 0 ) usage(); */

	/* now look for the printcap entry */
	Get_printer();
	if( All_printers ){
		Printer = "all";
		RemotePrinter = 0;
	}
	if( RemoteHost == 0 || *RemoteHost == 0 ){
		RemoteHost = 0;
		if( Default_remote_host && *Default_remote_host ){
			RemoteHost = Default_remote_host;
		} else if( FQDNHost && *FQDNHost ){
			RemoteHost = FQDNHost;
		}
	}

	DEBUG4("lprm: printer '%s', remote printer '%s', remote host '%s'",
		Printer, RemotePrinter, RemoteHost );
	if( RemoteHost == 0 ){
		Warnmsg( "No remote host specified" );
		usage();
		exit(1);
	}
	if( RemotePrinter == 0 || *RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}

	/*
	 * get the user name
	 */

	Logname = Get_user_information();

	Send_lprmrequest( RemotePrinter?RemotePrinter:Printer,
		RemoteHost, Logname, &argv[Optind], Connect_timeout, Send_timeout, 1 );
	exit(0);
}
