/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.c
 * PURPOSE:
 **************************************************************************/
static char *const _id =
"$Id: lprm.c,v 3.6 1997/03/24 00:45:58 papowell Exp papowell $";

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

#include "lp.h"
#include "initialize.h"
#include "killchild.h"
#include "sendlprm.h"
#include "getprinter.h"
/**** ENDINCLUDE ****/

char LPRM_optstr[]   /* LPRM options */
 = "aAD:P:V" ;


/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/

extern void usage(void);

int main(int argc, char *argv[], char *envp[])
{
	struct printcap_entry *printcap_entry = 0;
	/*
	 * set up the user state
	 */
	Interactive = 1;
	Initialize();


	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);


	/* scan the argument list for a 'Debug' value */

	Get_debug_parm( argc, argv, LPRM_optstr, debug_vars );

	/* setup configuration */
	Setup_configuration();

	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */
	/* need at least one argument */
	/*if( argc  - Optind <= 0 ) usage(); */

	/* now look for the printcap entry */
	Get_printer(&printcap_entry);
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

	DEBUG3("lprm: printer '%s', remote printer '%s', remote host '%s'",
		Printer, RemotePrinter, RemoteHost );
	if( RemoteHost == 0 ){
		Warnmsg( _("No remote host specified") );
		usage();
		exit(1);
	}
	if( RemotePrinter == 0 || *RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}

	Send_lprmrequest( RemotePrinter?RemotePrinter:Printer,
		RemoteHost, Logname, &argv[Optind], Connect_timeout, Send_timeout, 1 );
	Errorcode = 0;
	cleanup(0);
	return(0);
}
