/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkremote.c
 * PURPOSE: check for a remote printer
 **************************************************************************/

static char *const _id =
"checkremote.c,v 3.9 1997/09/18 19:45:52 papowell Exp";

#include "lp.h"
#include "checkremote.h"
#include "gethostinfo.h"
#include "printcap.h"
/**** ENDINCLUDE ****/


/***************************************************************************
Checkremotehost()
    check to see if we have a remote host specified by the LP_device name

 ***************************************************************************/

void Check_remotehost( void )
{
	char *s, *end;

	DEBUG3("Check_remotehost: before RemotePrinter '%s', RemoteHost '%s',  Lp '%s'",
		RemotePrinter, RemoteHost, Lp_device );
	Destination_port = 0;
	if( Lp_device ){
		if( (Lp_device[0] == '|') ){
			if( Is_server ){
				RemotePrinter = 0;
				RemoteHost = 0;
			}
		} else if( strchr(Lp_device,'@') ){
			/* we have printer@host[%port] */
			static char *rdup;
			if( rdup ){
				free(rdup);
				rdup = 0;
			}
			rdup = safestrdup( Lp_device );
			RemotePrinter = rdup;
			s = strchr( rdup, '@' );
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
				if( end == s || *end != 0 || Destination_port <= 0 ){
					fatal( LOG_ERR,
					"Check_remotehost: 'lp' entry has bad port number '%s'",
						Lp_device );
				}
			}
		}
	}
	DEBUG3("Check_remotehost: RemoteHost '%s', RemotePrinter '%s'",
		RemoteHost, RemotePrinter );
}

int Check_loop( void )
{
	int checkloop = 0;

	/*
	 * Detect loops by seeing if RemoteHost and RemotePrinter
	 * are the same as the Printer and current host
	 */
	if( RemoteHost && RemotePrinter ){
		checkloop = 1;
		FQDNRemote = Find_fqdn(&RemoteHostIP, RemoteHost, 0 );
		DEBUG3("Check_remotehost: checkloop RemoteHost '%s', FQDNHost '%s'",
			FQDNRemote, FQDNHost);
		if( FQDNRemote ){
			checkloop = !Same_host( &RemoteHostIP, &HostIP );
		}
		if( Printer && checkloop ){
			checkloop = !strcmp( RemotePrinter, Printer );
		}
	}
	DEBUG3("Check_loop: loop %d, RemoteHost '%s', RemotePrinter '%s'",
		checkloop, RemoteHost, RemotePrinter );
	return( checkloop );
}
