/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkremote.c
 * PURPOSE: check for a remote printer
 **************************************************************************/

static char *const _id =
"$Id: checkremote.c,v 3.0 1996/05/19 04:05:54 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "lp_config.h"


/***************************************************************************
Checkremotehost()
    check to see if we have a remote host specified by the LP_device name

 ***************************************************************************/

void Check_remotehost( int checkloop )
{
	char *s, *end;
	static char *rdup;

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
	/*
	* Prevent loops by clearing RemoteHost and RemotePrinter if they
	* point at us.
	*/
	if( checkloop && RemoteHost
		&& !strcmp(Find_fqdn(RemoteHost, Domain_name), FQDNHost)
		&& (RemotePrinter == 0 || !strcmp(RemotePrinter, Printer))) {
			RemoteHost = 0;
			RemotePrinter = 0;
	}
	if( RemoteHost ){
		if( RemotePrinter == 0 ){
			RemotePrinter = Printer;
		}
	}
}
