/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: gethostinfo.c
 * PURPOSE: host name information lookup
 * char *get_fqdn() - get fully qualified domain name
 *
 **************************************************************************/

static char *const _id =
"$Id: gethostinfo.c,v 3.0 1996/05/19 04:05:59 papowell Exp $";
/********************************************************************
 * char *get_fqdn (char *shorthost)
 * get the fully-qualified domain name for a host.
 *
 * NOTE: get_fqdn returns a pointer to static data, so copy the result!!
 * i.e.-  strcpy (fqhostname, get_fqdn (hostname));
 * 
 ********************************************************************/

#include "lp.h"
#include "lp_config.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

/********************************************************************
 * char *check_hostent ( char *shorthost, struct hostent *host_ent )
 * Checks the hostent data structure for a fully qualified domain name
 * This is indicated by the presence of a '.' in domain name
 *
 * ASSUMES: shorthost name is shorter than LINEBUFFER
 * WARNING: do not put anything in that could invoke gethostbyname, etc.
 * RETURNS: FQDN if found, 0 if not found
 ********************************************************************/
char *check_hostent ( char *shorthost, struct hostent *host_ent )
{
	char **alias;
	const char *s;			/* alias information from hostent */
	char *retv = 0;
	static char pattern[LINEBUFFER]; /* dataspace for pattern (if needed) */

	if( host_ent == 0 ) return( retv );
	s = host_ent->h_name;
	if (strchr (s, '.') == 0 ){
		/* we'll assume that if one of the aliases matches /^Shorthost\./,
		 * then it's the fully-qualified domain name. (ugh)
		 * Purify: if we get an ABR here, it's gethostbyname()'s fault, not
		 * ours.
		 */
		if (host_ent->h_aliases) {
			for (alias = host_ent->h_aliases; (s = *alias); alias++) {
				if( strchr( s, '.') ){
					DEBUG4 ("check_hostent: '%s' h_alias '%s'", shorthost,s );
					break;
				}
		    }
		}
	}
	if( s ){
		strncpy( pattern, s, sizeof(pattern) );
		retv = pattern;
	}
	return( retv );
}

/********************************************************************
 * char *find_fqdn ( char *shorthost )
 * Finds a fully quaulified domain name for a host
 * This is indicated by the presence of a '.' in domain name
 *
 * ASSUMES: shorthost name is shorter than LINEBUFFER
 * RETURNS: FQDN if found, 0 if not found
 ********************************************************************/
char *Find_fqdn ( char *shorthost, char *domain )
{
	struct hostent *host_ent;           /* host entry from name data base */
	static char *fqdn;
	int len;

	if( fqdn ) free(fqdn); fqdn = 0;
	if (strchr (shorthost, '.') != 0) {
		/* it's already fully-qualified, just return itself */
		fqdn = shorthost;
	} else {
		DEBUG4 ("FQDN for Hostname '%s': trying gethostbyname", shorthost);
		host_ent = gethostbyname( shorthost );
		fqdn = check_hostent( shorthost, host_ent );
		/* problem: some gethostbyname() implementations do not return FQDN
		 * apparently the gethostbyaddr does... This is really quite silly,
		 * but here is a work around
		 */
		if( !fqdn && host_ent ) {
				host_ent = gethostbyaddr(*host_ent->h_addr_list,
					host_ent->h_length,
					host_ent->h_addrtype);
				fqdn = check_hostent(shorthost, host_ent);
		}
	}
	if( fqdn == 0 ){
		DEBUG4("Find_fqdn: Hostname '%s': can't resolve", shorthost);
			DEBUG4 ("Find_fqdb '%s': appending domain name", shorthost);
			/* create shorthost.domainname, bompproof things */
			len = strlen( shorthost )+4;
			if( domain ) len += strlen( domain );
			malloc_or_die( fqdn, len );
			strcpy( fqdn, shorthost );
			if( domain && *domain ){
				strcat( fqdn, "." );
				strcat( fqdn, domain );
			}
	} else {
		fqdn = safestrdup(fqdn);
	}
	DEBUG4 ("Find_fqdb '%s': returning '%s'", shorthost, fqdn );
	return(fqdn);
}

/***************************************************************************
 * char *Get_Local_fqdn()
 * Get the fully-qualified hostname of the local host.
 * Tricky this; needs to be run after the config file has been read;
 * also, it depends a lot on the local DNS/NIS/hosts-file/etc. setup.
 *
 * Patrick Powell Fri Apr  7 07:47:23 PDT 1995
 * Note: the orginal code for this operation modified various global variables.
 * 1. we use the gethostname() call if it is available
 * 2. we use the uname() call if it is available
 * 3. we get the $HOST environment variable
 ***************************************************************************/

#if defined(HAVE_SYS_SYSTEMINFO_H)
# include <sys/systeminfo.h>
#endif

#ifndef HAVE_GETHOSTNAME
# if defined(HAVE_SYSINFO)
int gethostname( char *nbuf, long nsiz )
{
	int i;
	i = sysinfo(SI_HOSTNAME,nbuf, nsiz );
	DEBUG3 ("gethostname: using sysinfo '%s'", nbuf );
	return( i );
}
# else
# ifdef HAVE_UNAME
#   if defined(HAVE_SYS_UTSNAME_H)
#     include <sys/utsname.h>
#   endif
#   if defined(HAVE_UTSNAME_H)
#     include <utsname.h>
#   endif
int gethostname (char *nbuf, long nsiz)
{
	struct utsname unamestuff;  /* structure to catch data from uname */

	if (uname (&unamestuff) < 0) {
		return -1;
	}
	(void) max_strncpy (nbuf, unamestuff.nodename, nsiz);
	nbuf[nsiz-1] = 0;
	return( 0 );
}

# else
 
int gethostname (char *nbuf, long nsiz)
{
	char *name;
	name = getenv( "HOST" );
	if( name == 0 ){
		return( -1 );
	}
	(void) max_strncpy (nbuf, name, nsiz);
	nbuf[nsiz-1] = 0;
	return( 0 );
}

# endif /* HAVE_UNAME */
# endif /* HAVE_SYSINFO */
#endif /* HAVE_GETHOSTNAME */


char *Get_local_fqdn ( char *domain )
{
	char host[LINEBUFFER];
	char *fqdn;
	 /*
	  * get the Host computer Name
	  */
	host[0] = 0;
	if( gethostname (host, sizeof(host)) < 0 
		|| host[0] == 0 ) {
		fatal( LOG_ERR, "Get_local_fqdn: no host name" );
	}
	fqdn = Find_fqdn( host, domain );
	DEBUG3 ("Get_local_fqdn: short=%s, fqdn=%s", host, fqdn);
	return( fqdn );
}

/****************************************************************************
 * void Get_local_host()
 * 1. We try the usual method of getting the host name.
 *    This may fail on a PC based system where the host name is usually
 *    not available.
 * 2. If we have no host name,  then we try to use the IP address
 *    This will almost always work on a system with a single interface.
 ****************************************************************************/

void Get_local_host()
{
	char *host, *s;
	struct in_addr addr;		/* get address information */
	
	host = Get_local_fqdn( Domain_name );

	/* we now set the various variables: FQDN, ShortHost, Host */
	FQDNHost = safestrdup( host );
	ShortHost = safestrdup( host );
	/* truncate the ShortHost */
	if( (s = strchr( ShortHost, '.' )) ) *s = 0;
	addr.s_addr = HostIP = Find_ip( host );
	DEBUG3 ("Get_local_host: ShortHost=%s, FQDNHost=%s, HostIP = %s (0x%08x)",
		ShortHost, FQDNHost, inet_ntoa( addr ), HostIP );
}

/***************************************************************************
 * unsigned long Find_ip( char *shorthost )
 *  Find the IP address for the host name
 *  returns 0 if not found
 ***************************************************************************/
unsigned long Find_ip( char *shorthost )
{
	struct hostent *host_ent;  /* host entry from name data base */
	struct in_addr *addr;		/* get address information */
	unsigned long ip = 0;

	host_ent = gethostbyname( shorthost );
	if( host_ent ){
		addr = (struct in_addr *)(host_ent->h_addr_list[0]);
		ip = addr->s_addr;
	}
	return( ip );
}

/***************************************************************************
 * char *Get_realhostname( struct control_file *cfp )
 *  Find the real hostname of the job
 *  sets the cfp->realhostname entry as well
 ***************************************************************************/

char *Get_realhostname( struct control_file *cfp )
{
	char *s;

	/* check to see if set */
	if( cfp->realhostname == 0 ){
		DEBUG8("Get_realhostname: looking up '%s'",
			cfp->FROMHOST+1 );
		if( cfp->FROMHOST[1] ){
			s = Find_fqdn( cfp->FROMHOST+1, Domain_name );
		} else {
			s = "";
		}
		cfp->realhostname = add_buffer( &cfp->control_file, strlen(s)+1 );
		strcpy( cfp->realhostname, s );
	}
	DEBUG8("Get_realhostname: found '%s'", cfp->realhostname );
	return( cfp->realhostname );
}
