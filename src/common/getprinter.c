/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getprinter.c
 * PURPOSE: get the printer and remote host name
 **************************************************************************/

static char *const _id =
"getprinter.c,v 3.10 1997/09/18 19:45:58 papowell Exp";

#include "lp.h"
#include "getprinter.h"
#include "printcap.h"
#include "checkremote.h"
#include "dump.h"
/**** ENDINCLUDE ****/

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
void Get_printer( struct printcap_entry **pce )
{
	char *s;

	DEBUG0("Get_printer: original printer '%s'", Printer );
	if( Printer == 0 ){
		Printer = getenv( "PRINTER" );
		/* Sigh... some folks want one for Solaris, one for LPRng... ok */
		if( Printer == 0 && (s = getenv( "LPDEST" )) ) Printer = s;
		if( (s = getenv( "NGPRINTER" )) ) Printer = s;
	}

	/* see if there is something in the printcap file */
	if( Printer == 0 ){
		Printer = Get_first_printer();
	}
	if( Printer == 0 || *Printer == 0 ){
		Printer = Default_printer;
	}
	if( Printer && (s = strchr( Printer, '\n' )) ) *s = 0;
	if( Printer == 0 || *Printer == 0 ){
		fatal( LOG_ERR, "Get_printer: no printer name available" );
	}

	/* now we try getting the printcap entry */
	Fix_remote_name( pce );
	DEBUG0("Get_printer: printer now '%s', Is_server %d, Force_localhost %d",
		Printer, Is_server, Force_localhost );

	if(DEBUGL1)dump_parms("Get_printer",Pc_var_list);
}

/***************************************************************************
 * Fix_remote_name( int cyclecheck )
 *  - check the printer name for printer@remote and fix it up
 *    set RemoteHost and RemotePrinter
 ***************************************************************************/
void Fix_remote_name( struct printcap_entry **pce )
{
	static char *sdup;
	static char *pdup;
	char *s;

	DEBUG0("Fix_remote_name: printer name '%s'", Printer );
	if( pdup ) free( pdup ); pdup = 0;
	if( sdup ) free( sdup ); sdup = 0;
	if( Queue_name ){
		Queue_name = sdup = safestrdup( Queue_name );
	} else {
		Queue_name = sdup = safestrdup( Printer );
	}
	Printer = pdup = safestrdup( Printer );

	/*
	 * now check to see if we have a remote printer
	 * 1. printer@host form overrides
	 * 2. printcap entry, we use lp=pr@host
	 * 3. printcap entry, we use remote host, remote printer
	 * 4. no printcap entry, we use default printer, default remote host
	 */

	if( (s = strchr( Queue_name, '@' ))  ){
		*s++ = 0;
		Printer = Queue_name;
		Lp_device = pdup;
	} else if( (s = strchr( Printer, '@' ))  ){
		Lp_device = pdup;
	} else if( (s = Get_printer_vars( Printer, pce )) ){
		Printer = s;
		Expand_value( Pc_var_list, &Raw_printcap_files );
	}

	/* extract the RemoteHost if in the Lp_device */
	if( Lp_device && strchr( Lp_device, '@' ) ){
		Check_remotehost();
	}

	if( RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}
	if( RemoteHost == 0 || *RemoteHost == 0 ){
		RemoteHost = 0;
		if( Default_remote_host && *Default_remote_host ){
			RemoteHost = Default_remote_host;
		} else if( FQDNHost && *FQDNHost ){
			RemoteHost = FQDNHost;
		}
	}
	if( !Is_server && Force_localhost ){
		RemoteHost = Localhost;
	}

	DEBUG0(
		"Fix_remote_name: Queue_name '%s', Printer '%s', RemotePrinter '%s', RemoteHost '%s'",
		Queue_name, Printer, RemotePrinter, RemoteHost );
}
