/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.c
 * PURPOSE:
 **************************************************************************/
static char *const _id =
"lprm.c,v 3.14 1998/03/24 02:43:22 papowell Exp";

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
#include "printcap.h"
#include "checkremote.h"
#include "permission.h"
#include "linksupport.h"
/**** ENDINCLUDE ****/


/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/

extern void usage(void);

int main(int argc, char *argv[], char *envp[])
{
	char **list;
	int i;
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



	/* setup configuration */
	Setup_configuration();

	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */
	/* need at least one argument */
	/*if( argc  - Optind <= 0 ) usage(); */

	/* now look for the printcap entry */
	Get_printer(0);
	if( All_printers || (Printer && strcmp(Printer,"all") == 0 ) ){
		Get_all_printcap_entries();
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

	if( RemotePrinter == 0 || *RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}

	DEBUG0("lprm: remoteprinter '%s', remote host '%s'",
		RemotePrinter, RemoteHost );

	if( All_printers && All_list.count ){
		list = All_list.list;
		for( i = 0; i < All_list.count; ++i ){
			RemoteHost = RemotePrinter = Lp_device = 0;
			Printer = list[i];
			DEBUG0("lprm: Printer [%d of %d] '%s'",
				i, All_list.count, Printer );
			if( strchr( Printer, '@' ) ){
				Lp_device = Printer;
				Check_remotehost();
			} else if( Force_localhost ){
				RemoteHost = Localhost;
			}
			if( RemoteHost == 0 || *RemoteHost == 0 ){
				if( Default_remote_host && *Default_remote_host ){
					RemoteHost = Default_remote_host;
				} else if( FQDNHost && *FQDNHost ){
					RemoteHost = FQDNHost;
				}
			}
			if( Check_for_rg_group( Logname ) ){
				Warnmsg( "cannot use printer - not in privileged group\n" );
			}
			if( RemotePrinter && RemotePrinter[0] == 0 ) RemotePrinter = 0;
			Send_lprmrequest( RemotePrinter?RemotePrinter:Printer,
				RemoteHost, Logname, &argv[Optind], Connect_timeout,
				Send_query_rw_timeout, 1 );
		}
	} else {
		DEBUG3("lprm: printer '%s', remote printer '%s', remote host '%s'",
			Printer, RemotePrinter, RemoteHost );
		if( RemoteHost == 0 ){
			Warnmsg( _("No remote host specified") );
			usage();
			exit(1);
		}
		if( Check_for_rg_group( Logname ) ){
			Warnmsg( "cannot use printer - not in privileged group\n" );
			Errorcode = 1;
			cleanup(0);
		}
		Send_lprmrequest( RemotePrinter?RemotePrinter:Printer,
			RemoteHost, Logname, &argv[Optind], Connect_timeout,
			Send_query_rw_timeout, 1 );
	}
	Errorcode = 0;
	cleanup(0);
	return(0);
}
