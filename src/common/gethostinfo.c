/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
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
"$Id: gethostinfo.c,v 3.8 1997/01/30 21:15:20 papowell Exp $";
/********************************************************************
 * char *get_fqdn (char *shorthost)
 * get the fully-qualified domain name for a host.
 *
 * NOTE: get_fqdn returns a pointer to static data, so copy the result!!
 * i.e.-  strcpy (fqhostname, get_fqdn (hostname));
 * 
 ********************************************************************/

#include "lp.h"
#include "gethostinfo.h"
#include "malloclist.h"
#include "dump.h"
/**** ENDINCLUDE ****/

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif


/***************************************************************************
 * void Check_for_dns_hack( struct hostent *h_ent )
 * Check to see that you do not have some whacko type returned by DNS
 ***************************************************************************/
void Check_for_dns_hack( struct hostent *h_ent )
{
	int count = 1;
	switch( h_ent->h_addrtype ){
	case AF_INET:
		count = (h_ent->h_length != sizeof(struct in_addr )); break;
#if defined(IN6_ADDR)
	case AF_INET6:
		count = (h_ent->h_length != sizeof(struct in6_addr)); break;
#endif
	}
	if( count ){
		fatal( LOG_ALERT,
		"Check_for_dns_hack: HACKER ALERT! DNS address length wrong, prot %d len %d",
			h_ent->h_addrtype, h_ent->h_length );
	}
}

/********************************************************************
 * char *Find_fqdn (
 * struct host_informaiton *info - we put information here
 * char *shorthost - hostname
 *
 * Finds IP address and fully qualified domain name for a host
 *
 * ASSUMES: shorthost name is shorter than LINEBUFFER
 * RETURNS: FQDN if found, 0 if not found
 ********************************************************************/
char *Find_fqdn( struct host_information *info, const char *shorthost,
	struct hostent *host_ent )
{
	char **list, *s;
	char *fqdn;
	int count;

	DEBUG0( "Find_fqdn: host '%s', host_ent 0x%x", shorthost, host_ent );
	clear_malloc_list( &info->host_names, 1 );
	clear_malloc_list( &info->host_addr_list, 0 );
	info->shorthost[0] = 0;
	info->fqdn = 0;

	if( shorthost == 0 || *shorthost == 0 ){
		log( LOG_ALERT, "Find_fqdn: called with '%s', HACKER ALERT",
			shorthost );
		return(0);
	}
	if( strlen(shorthost) > 64 ){
		fatal( LOG_ALERT, "Find_fqdn: hostname too long, HACKER ALERT '%s'",
			shorthost );
	}
	if( host_ent == 0 ){
#if defined(HAVE_GETHOSTBYNAME2)
		host_ent = gethostbyname2( shorthost, AF_Protocol );
#else
		host_ent = gethostbyname( shorthost );
#endif
	}
	if( host_ent == 0 ){
		DEBUGF(DNW1)( "Find_fqdn: no entry for host '%s'", shorthost );
		return( 0 );
	}
	/* sigh... */
	Check_for_dns_hack(host_ent);

	/* problem: some gethostbyname() implementations do not return FQDN
	 * apparently the gethostbyaddr does... This is really quite silly,
	 * but here is a work around
	 * LINUX BRAIN DAMAGE - as of Version 4.0 RedHat, Jan 2, 1997
	 * it has been observed that the LINUX gethostbyname() clobbers
	 * buffers returned by gethostbyaddr() BEFORE they are
	 * used and this is not documented.  This has the side effect that using
	 * buffers returned by gethostbyname to gethostbyaddr() calls will
	 * get erroneous results,  and in addition will also modify the original
	 * values in the structures pointed to by gethostbyaddr.
	 *
	 * After the call to gethostbyaddr,  you will need to REPEAT the call to
	 * gethostbyname()...
	 *
	 * This implementation of gethostbyname()/gethostbyaddr() violates an
	 * important principle of library design, which is functions should NOT
	 * interact, or if they do, they should be CLEARLY documented.
	 *
	 * To say that I am not impressed with this is a severe understatement.
	 * Patrick Powell, Jan 29, 1997
	 */
	fqdn = strchr( host_ent->h_name, '.' );
	for( list = host_ent->h_aliases;
		fqdn == 0 && list && *list; ++list ){
		fqdn = strchr( *list, '.' );
	}
	if( fqdn == 0 ){
		char buffer[64];
		struct sockaddr temp_sockaddr;
		memcpy( &temp_sockaddr, *host_ent->h_addr_list, host_ent->h_length );
		DEBUGF(DNW3)(
		"Find_fqdn: using gethostbyaddr for host '%s', addr '%s'",
		host_ent->h_name, inet_ntop( host_ent->h_addrtype,
			*host_ent->h_addr_list, buffer, sizeof(buffer)) );
		host_ent = gethostbyaddr( (void *)&temp_sockaddr,
			host_ent->h_length, host_ent->h_addrtype );
		if( host_ent ){
			/* sigh... */
			Check_for_dns_hack(host_ent);

			DEBUGF(DNW3)(
			"Find_fqdn: gethostbyaddr found host '%s', addr '%s'",
				host_ent->h_name,
				inet_ntop( host_ent->h_addrtype,
				*host_ent->h_addr_list, buffer,
				sizeof(buffer)) );
			fqdn = strchr( host_ent->h_name, '.' );
			for( list = host_ent->h_aliases;
				fqdn == 0 && list && *list; ++list ){
				fqdn = strchr( *list, '.' );
			}
		}
	}
	/* we have to do the lookup AGAIN */
	if( host_ent == 0 ){
#if defined(HAVE_GETHOSTBYNAME2)
		host_ent = gethostbyname2( shorthost, AF_Protocol );
#else
		host_ent = gethostbyname( shorthost );
#endif
		/* sigh... */
		Check_for_dns_hack(host_ent);
	}

	/* make a copy of all of the names */
	info->host_names.count = 0;
	if( info->host_names.count+1 >= info->host_names.max ){
		extend_malloc_list( &info->host_names, sizeof( list[0] ),
			info->host_names.count+10 );
	}
	fqdn = 0;
	s = strcpy( add_buffer( &info->host_names, strlen(host_ent->h_name)+1 ),
			host_ent->h_name );
	if( strchr( s, '.' ) ) fqdn = s;
	for( list = host_ent->h_aliases; list && *list; ++list ){
		s = strcpy( add_buffer( &info->host_names, strlen(*list)+1 ), *list );
		if( fqdn == 0 && strchr( s, '.' ) ) fqdn = s;
	}
	if( info->host_names.count+1 >= info->host_names.max ){
		extend_malloc_list( &info->host_names, sizeof( list[0] ),
			info->host_names.count+10 );
	}
	info->host_names.list[info->host_names.count] = 0;
	if( fqdn == 0 ){
		/* use the first in the list */
		fqdn = info->host_names.list[0];
	}

	info->fqdn = fqdn;
	safestrncpy( info->shorthost, fqdn );
	if( (s = strchr( info->shorthost, '.' ) ) ) *s = 0;


	if( info->host_addrtype != host_ent->h_addrtype
		|| info->host_addrlength != host_ent->h_length ){
		if( info->host_addr_list.list ){
			free( info->host_addr_list.list );
		}
		memset( &info->host_addr_list, 0, sizeof(info->host_addr_list ));
	}
	info->host_addrtype = host_ent->h_addrtype;
	info->host_addrlength = host_ent->h_length;
	count = 0;
	for( list = host_ent->h_addr_list; list && list[count]; ++count );
	if( count == 0 ){
		fatal(LOG_ERR,"Find_fqdn: '%s' does not have address", shorthost );
	}

	if( count > info->host_addr_list.max ){
		extend_malloc_list( &info->host_addr_list, info->host_addrlength,
			count+10 );
	}
	s = (void *)info->host_addr_list.list;
	count = 0;
	for( list = host_ent->h_addr_list; list && list[count]; ++count ){
		memcpy( s, list[count], host_ent->h_length );
		s += host_ent->h_length;
	}
	info->host_addr_list.count = count;
	DEBUGFC(DNW3) dump_host_information( "Find_fqdn", info );

	DEBUGF(DNW3) ("Find_fqdn '%s': returning '%s'", shorthost, fqdn );
	return(fqdn);
}

/***************************************************************************
 * char *Get_local_host()
 * Get the fully-qualified hostname of the local host.
 * Tricky this; needs to be run after the config file has been read;
 * also, it depends a lot on the local DNS/NIS/hosts-file/etc. setup.
 *
 * Patrick Powell Fri Apr  7 07:47:23 PDT 1995
 * 1. we use the gethostname() call if it is available
 *    If we have the sysinfo call, we use it instead.
 * 2. we use the uname() call if it is available
 * 3. we get the $HOST environment variable
 ***************************************************************************/

#if defined(HAVE_SYS_SYSTEMINFO_H)
# include <sys/systeminfo.h>
#endif

#if !defined(HAVE_GETHOSTNAME_DEF)
# ifdef HAVE_GETHOSTNAME
int gethostname (char *nbuf, long nsiz);
# else
#  if defined(HAVE_SYSINFO)
int gethostname( char *nbuf, long nsiz )
{
	int i;
	i = sysinfo(SI_HOSTNAME,nbuf, nsiz );
	DEBUGF(DNW3) ("gethostname: using sysinfo '%s'", nbuf );
	return( i );
}
#  else
#   ifdef HAVE_UNAME
#      if defined(HAVE_SYS_UTSNAME_H)
#        include <sys/utsname.h>
#      endif
#      if defined(HAVE_UTSNAME_H)
#        include <utsname.h>
#      endif
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

#   else
 
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

#   endif /* HAVE_UNAME */
#  endif /* HAVE_SYSINFO */
# endif /* HAVE_GETHOSTNAME */
#endif /* HAVE_GETHOSTNAME_DEF */

/****************************************************************************
 * void Get_local_host()
 * 1. We try the usual method of getting the host name.
 *    This may fail on a PC based system where the host name is usually
 *    not available.
 * 2. If we have no host name,  then we try to use the IP address
 *    This will almost always work on a system with a single interface.
 ****************************************************************************/

void Get_local_host( void )
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
	fqdn = Find_fqdn( &HostIP, host, 0 );
	DEBUGF(DNW3) ("Get_local_host: fqdn=%s", fqdn);
	if( fqdn == 0 ){
		fatal( LOG_ERR, "Get_local_host: hostname '%s' bad", host );
	}
	FQDNHost = HostIP.fqdn;
	ShortHost = HostIP.shorthost;
	DEBUGF(DNW3) ("Get_local_host: ShortHost=%s, FQDNHost=%s",
		ShortHost, FQDNHost);
}

/***************************************************************************
 * void Get_remote_hostbyaddr( struct sockaddr *sin );
 * 1. look up the address using gethostbyaddr()
 * 2. if not found, we have problems
 ***************************************************************************/
 
char *Get_remote_hostbyaddr( struct host_information *info,
	struct sockaddr *sin )
{
	struct hostent *host_ent;
	void *addr = 0;
	int len = 0; 
	char *fqdn = 0;
	char *s;

	FQDNRemote = ShortRemote = 0;
	DEBUGFC(DNW3){
		char buffer[64];
		logDebug("Get_remote_hostbyaddr: %s",
			inet_ntop_sockaddr( sin, buffer, sizeof(buffer) ) );
	}
	if( sin->sa_family == AF_INET ){
		addr = &((struct sockaddr_in *)sin)->sin_addr;
		len = sizeof( ((struct sockaddr_in *)sin)->sin_addr );
#if defined(IN6_ADDR)
	} else if( sin->sa_family == AF_INET6 ){
		addr = &((struct sockaddr_in6 *)sin)->sin6_addr;
		len = sizeof( ((struct sockaddr_in6 *)sin)->sin6_addr );
#endif
	} else {
		fatal( LOG_ERR, "Get_remote_hostbyaddr: bad family '%d'",
			sin->sa_family);
	}
	host_ent = gethostbyaddr( addr, len, sin->sa_family );
	if( host_ent ){
		fqdn = Find_fqdn( info, host_ent->h_name, host_ent );
	} else {
		/* We will need to create a dummy record. - no host */
		info->shorthost[0] = 0;
		info->fqdn = 0;
		info->host_names.count = 0;
		info->host_addr_list.count = 0;
		if( info->host_addrtype != sin->sa_family
			|| info->host_addrlength != len ){
			if( info->host_addr_list.list ){
				free( info->host_addr_list.list );
			}
			memset( &info->host_addr_list, 0, sizeof(info->host_addr_list ));
		}
		info->host_addrtype = host_ent->h_addrtype;
		info->host_addrlength = len;

		/* put in the address information */
		if( 1 > info->host_addr_list.max ){
			extend_malloc_list( &info->host_addr_list, len, 1 );
		}
		s = (void *)info->host_addr_list.list;
		memcpy( s, addr, len );
		info->host_addr_list.count = 1;
		fqdn = info->shorthost;
	}
	DEBUGF(DNW3)("Get_remote_hostbyaddr: %s", fqdn );
	FQDNRemote = info->fqdn;
	ShortRemote = info->shorthost;
	DEBUGFC(DNW3) dump_host_information( "Find_fqdn", info );
	return( fqdn );
}

/***************************************************************************
 * int Same_host( struct host_information *h1, *h2 )
 *  returns 1 on failure, 0 on success
 *  - compares the host addresses for an identical one
 ***************************************************************************/
int Same_host( struct host_information *host,
	struct host_information *remote )
{
	int i, j;
	char *h1, *h2;
	int c1, c2, l1, l2;
	int result = 1;

	if( host && remote ){
		h1 = (void *)host->host_addr_list.list;
		c1 = host->host_addr_list.count;
		l1 = host->host_addrlength;
		h2 = (void *)remote->host_addr_list.list;
		c2 = remote->host_addr_list.count;
		l2 = remote->host_addrlength;
		if( l1 == l2 ){ 
			for( i = 0; result && i < c1; ++i ){
				for( j = 0; result && j < c2; ++j ){
					result = memcmp( h1+(i*l1), h2+(j*l2), l1 );
				}
			}
		}
	}
	return( result != 0 );
}
