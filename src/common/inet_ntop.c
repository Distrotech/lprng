/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: link_support.c
 * PURPOSE: open and close data links to a remote host
 **************************************************************************/

static char *const _id =
"$Id: inet_ntop.c,v 1.4 1997/01/30 21:15:20 papowell Exp $";

#include "lp.h"

#if !defined(HAVE_INET_PTON)
/***************************************************************************
 * inet_pton( int family, const char *strptr, void *addrptr
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * strptr = string to convert
 * addrprt = destination
 ***************************************************************************/

int inet_pton( int family, const char *strptr, void *addr )
{
	if( family != AF_INET ){
		fatal( LOG_ERR, "inet_pton: bad family '%d'", family );
	}
#if defined(HAVE_INET_ATON)
	return( inet_aton( strptr, addr ) );
#else
#if !defined(INADDR_NONE)
#define INADDR_NONE (-1)
#endif
	if( inet_addr( strptr ) != INADDR_NONE ){
		((unsigned long *)addr)[0] = inet_addr( strptr );
		return(1);
	}
	return(0);
#endif
}
#endif


#if !defined(HAVE_INET_NTOP)
/***************************************************************************
 * inet_ntop( int family, const void *addr,
 *    const char *strptr, int len )
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * addr   = binary to convert
 * strptr = string where to place
 * len    = length
 ***************************************************************************/
char *inet_ntop( int family, const void *addr,
	char *str, int len )
{
	char *s;
	if( family != AF_INET ){
		fatal( LOG_ERR, "inet_ntop: bad family '%d'", family );
	}
	s = inet_ntoa(((struct in_addr *)addr)[0]);
	strncpy( str, s, len );
	return(str);
}
#endif

/***************************************************************************
 * inet_ntop_sockaddr( struct sockaddr *addr,
 *    const char *strptr, int len )
 * get the address type and format it correctly
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * addr   = binary to convert
 * strptr = string where to place
 * len    = length
 ***************************************************************************/
char *inet_ntop_sockaddr( struct sockaddr *addr,
	char *str, int len )
{
	void *a = 0;
	/* we get the printable from the socket address */
	if( addr->sa_family == AF_INET ){
		a = &((struct sockaddr_in *)addr)->sin_addr;
#if defined(IN6_ADDR)
	} else if( addr->sa_family == AF_INET6 ){
		a = &((struct sockaddr_in6 *)addr)->sin6_addr;
#endif
	} else {
		fatal( LOG_ERR, "inet_ntop_sockaddr: bad family '%d'",
			addr->sa_family );
	}
	return( (void *)inet_ntop( addr->sa_family, a, str, len ) );
}
