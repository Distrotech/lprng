/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getprinter.c
 * PURPOSE: get the printer and remote host name
 **************************************************************************/

static char *const _id =
"$Id: getprinter.c,v 3.0 1996/05/19 04:06:01 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "lp_config.h"
#include "getprinter.h"

/***************************************************************************
Get_printer()
    determine the name of the printer - Printer variable
	Note: this is used by clients to find the name of default printer
	or by server to find forwarding information.  If the printcap
	RemotePrinter is specified this overrides the printer name.
	1. -P option
	2. $PRINTER argument variable
	3. printcap file
	4. "lp" if none specified
	5. Get the printcap entry (if any),  and re-extract information-
        - printer name (primary name)
		- lp=printer@remote or rp@rm information
    6. recheck the printer name for printer@hostname form,
       and set RemoteHost to the hostname
	Note: this appears to cover all the cases, with the exception that
	a primary name of the form printer@host will be detected as the
	destination.  Sigh...
 ***************************************************************************/
void Get_printer()
{
	char *s;


	DEBUG4("Get_printer: start printer '%s'", Printer );
	if( Printer == 0 ){
		Printer = getenv( "PRINTER" );
		/* Sigh... some folks want one for Solaris, one for LPRng... ok */
		s = getenv( "NGPRINTER" );
		if( s ) Printer = s;
	}

	/* see if there is something in the printcap file */
	if( Printer == 0 && Printcapfile.init == 0
		&& Printcap_path && *Printcap_path ){
		DEBUG4("Get_printer: reading printcap '%s'", Printcap_path );
		Getprintcap( &Printcapfile, Printcap_path, 0 );
	}
	if( Printer == 0 || *Printer == 0 ){
		Printer = Get_first_printer( &Printcapfile );
	}
	if( Printer == 0 || *Printer == 0 ){
		Printer = Default_printer;
	}
	if( Printer == 0 || *Printer == 0 ){
		fatal( LOG_ERR, "Get_printer: no printer name available" );
	}

	if( (s = strchr( Printer, '\n' )) ) *s = 0;
	Fix_remote_name( 0 );
}

/***************************************************************************
 * Fix_remote_name( int cyclecheck )
 *  - check the printer name for printer@remote and fix it up
 *    set RemoteHost and RemotePrinter
 ***************************************************************************/
void Fix_remote_name( int cyclecheck )
{
	static char *sdup;
	static char *pdup;
	char error[LINEBUFFER];
	char *s;

	if( pdup ) free( pdup ); pdup = 0;
	if( sdup ) free( sdup ); sdup = 0;
	Queue_name = sdup = safestrdup( Printer );
	Printer = pdup = safestrdup( Printer );

	/*
	 * now check to see if we have a remote printer
	 * 1. printer@host form overrides
	 * 2. printcap entry, we use lp=pr@host
	 * 3. printcap entry, we use remote host, remote printer
	 * 4. no printcap entry, we use default printer, default remote host
	 */

	DEBUG4("Fix_remote_name: printer name '%s'", Printer );

	if( (s = strchr( Queue_name, '@' ))  ){
		*s++ = 0;
		Printer = Queue_name;
		Lp_device = pdup;
		Check_remotehost( cyclecheck );
	} else {
		if( Printcapfile.init == 0
			&& Printcap_path && *Printcap_path ){
			Getprintcap( &Printcapfile, Printcap_path, 0 );
		}
		s = Get_printer_vars( Printer, error, sizeof(error),
			&Printcapfile, &Pc_var_list, Default_printcap_var,
			(void *)0 );
		if( s ){
			Printer = s;
			Check_remotehost( cyclecheck );
		}
	}

	if( RemotePrinter && (RemoteHost == 0 || *RemoteHost == 0) ){
		RemoteHost = 0;
		if( Default_remote_host && *Default_remote_host ){
			RemoteHost = Default_remote_host;
		} else if( FQDNHost && *FQDNHost ){
			RemoteHost = FQDNHost;
		}
	}

	DEBUG7(
		"Fix_remote_name: Queue_name '%s', Printer '%s',"
		" RemotePrinter '%s', RemoteHost '%s'",
		Queue_name, Printer, RemotePrinter, RemoteHost );
}
