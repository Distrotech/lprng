/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkremote.c
 * PURPOSE: check for a remote printer
 **************************************************************************/

static char *const _id =
"$Id: checkremote.c,v 3.3 1997/01/19 14:34:56 papowell Exp $";

#include "lp.h"
#include "checkremote.h"
#include "gethostinfo.h"
#include "printcap.h"
/**** ENDINCLUDE ****/


/***************************************************************************
Checkremotehost()
    check to see if we have a remote host specified by the LP_device name

 ***************************************************************************/

void Check_remotehost( int checkloop )
{
	char *s, *end;
	static char *rdup;

	DEBUG3("Check_remotehost: checkloop %d, Lp '%s'",
		checkloop, Lp_device );
	Destination_port = 0;
	if( Lp_device && (s = strchr(Lp_device,'@'))){
		if( rdup ){
			free(rdup);
			rdup = 0;
		}
		rdup = safestrdup( Lp_device );
		/* we pick up the remote printer and host */
		RemotePrinter = rdup;
		if( (s = strchr( rdup, '@' )) ){
			*s++ = 0;
			RemoteHost = s;
			if( *RemoteHost == 0 ){
				fatal( LOG_ERR,
					"Check_remotehost: 'lp' entry missing host '%s'",
					Lp_device );
			}
			if( (s = strchr( RemoteHost, '%' )) ){
				*s++ = 0;
				end = s;
				Destination_port = strtol( s, &end, 10 );
				if( ((end - s) != strlen( s )) || Destination_port < 0 ){
				fatal( LOG_ERR,
					"Check_remotehost: 'lp' entry has bad port number '%s'",
						Lp_device );
				}
			}
		}
	}
	DEBUG3("Check_remotehost: RemoteHost '%s', RemotePrinter '%s'",
		RemoteHost, RemotePrinter );
	/*
	 * Prevent loops by clearing RemoteHost and RemotePrinter if they
	 * point at us.
	 */
	if( checkloop ){
		if( RemoteHost ){
			FQDNRemote = Find_fqdn(&RemoteHostIP, RemoteHost, 0 );
			DEBUG3("Check_remotehost: checkloop RemoteHost '%s', FQDNHost '%s'",
				FQDNRemote, FQDNHost);
		}
		if( checkloop && FQDNRemote ){
			checkloop = !strcmp( FQDNRemote, FQDNHost );
		}
		if( checkloop && RemotePrinter && Printer ){
			checkloop = !strcmp( RemotePrinter, Printer );
		}
		if( checkloop ){
			RemoteHost = 0;
			RemotePrinter = 0;
		}
	}
	if( RemoteHost && RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}
	DEBUG3("Check_remotehost: loop %d, RemoteHost '%s', RemotePrinter '%s'",
		checkloop, RemoteHost, RemotePrinter );
}
