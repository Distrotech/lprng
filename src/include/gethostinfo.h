/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: gethostinfo.h
 * PURPOSE: gethostinfo.c functions
 * gethostinfo.h,v 3.3 1997/01/22 23:09:32 papowell Exp
 **************************************************************************/

#ifndef _GETHOSTINFO_H
#define _GETHOSTINFO_H

/*****************************************************************
 * Get Fully Qualified Domain Name (FQDN) host name
 * The 'domain' information is appended to the host name only in
 * desperation cases and only if it is non-null.
 * If the FQDN cannot be found,  then you fall back on the host name.
 *****************************************************************/

char *Find_fqdn( struct host_information *info, const char *shorthost,
	struct hostent *host_ent );
void Get_local_host( void );
char *Get_remote_hostbyaddr( struct host_information *info,
	struct sockaddr *sin );
int Same_host( struct host_information *h1, struct host_information *h2 );

#endif
