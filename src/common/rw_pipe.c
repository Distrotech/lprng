/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: rw_pipe.c
 * PURPOSE: create a set of read/write pipe descriptors
 **************************************************************************/

static char *const _id = "rw_pipe.c,v 3.5 1997/09/18 19:46:04 papowell Exp";

#include "lp.h"
#include "rw_pipe.h"
/**** ENDINCLUDE ****/

/*
 * socketpair is not supported on some OSes, including SVR4 (according
 * to "Advanced Programming in the UNIX Environment", Stevens); however,
 * SVR4 uses pipe() to do the same thing, so we can support that here.
 *
 * Sat Aug  5 23:55:37 PDT 1995 Patrick Powell
 * Ummm... you usually find this out the hard way, but there
 * are some systems that do not have bidirectional sockets.
 * The following code fakes them out using UDP connections.
 * Now I KNOW that some of you are whooping your cookies at
 * this point,  but I will simply point out that it works.
 * Now some of you might have broken localhost addresses
 * (You know who I mean, ULTRIX fans), so please fix the
 * /etc/hosts entry with 127.0.0.1 localhost loopback
 */
int rw_pipe( int fds[] )
{
#if !defined(USE_RWSOCKETS) 
	int status;
#ifdef HAVE_SOCKETPAIR
	DEBUG4("rw_pipe: using socketpair" );
	status =  socketpair( AF_UNIX, SOCK_STREAM, 0, fds);
#else
	DEBUG4("rw_pipe: using pipe" );
	status = pipe( fds );
#endif
	if( status >= 0 ) status = 0;
	return( status );
#else
	int p1, p2;	/* read/write fds */
	struct sockaddr_in sin;     /* inet socket address */
	struct hostent *hostent;
	char *s;
	int i;

	memset(&sin, 0, sizeof (sin));
	sin.sin_family = LocalHostIP.host_addrtype;
	memcpy( &sin.sin_addr, (void *)LocalHostIP.host_addr_list.list,
		LocalHostIP.host_addrlength );

	p1 = socket(sin.sin_family, SOCK_DGRAM, 0);
	p2 = socket(sin.sin_family, SOCK_DGRAM, 0);
	if( bind(p1, (struct sockaddr *)&sin, sizeof(sin)) < 0 ){
		logerr( LOG_ERR, "rw_pipe: bind failed" );
		return(-1);
	}
	i = sizeof( sin );
	if( getsockname( p1, (struct sockaddr *)&sin, &i ) < 0 ){
		logerr(LOG_ERR,"rw_pipe: getsockname failed" );
		i = errno;
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}
	DEBUG3( "rw_pipe: sock %d, orig port %d, ip %s", p1,
		ntohs( sin.sin_port ), inet_ntoa( sin.sin_addr ) );

	if( connect( p2, (struct sockaddr *)&sin, sizeof(struct sockaddr) ) < 0 ){
		i = errno;
		logerr(LOG_ERR,"rw_pipe: fd %d connect failed", p2 );
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}

	i = sizeof( sin );
	if( getpeername( p2, (struct sockaddr *)&sin, &i ) < 0 ){
		logerr(LOG_ERR,"rw_pipe: getpeername failed" );
		i = errno;
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}
	DEBUG3( "rw_pipe: sock %d, dest port %d, ip %s", p2,
		ntohs( sin.sin_port ), inet_ntoa( sin.sin_addr ) );

	if( getsockname( p2, (struct sockaddr *)&sin, &i ) < 0 ){
		logerr(LOG_ERR,"rw_pipe: fd %d getsockname failed", p2 );
		i = errno;
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}
	if( connect( p1, (struct sockaddr *)&sin, sizeof(struct sockaddr) ) < 0 ){
		i = errno;
		logerr(LOG_ERR,"rw_pipe: fd %d connect failed", p1 );
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}
	i = sizeof( sin );
	if( getpeername( p1, (struct sockaddr *)&sin, &i ) < 0 ){
		logerr(LOG_ERR,"rw_pipe: getpeername failed" );
		i = errno;
		close( p1 );
		close( p2 );
		errno = i;
		return(-1);
	}
	DEBUG3( "rw_pipe: sock %d, dest port %d, ip %s", p1,
		ntohs( sin.sin_port ), inet_ntoa( sin.sin_addr ) );

	fds[0] = p1;
	fds[1] = p2;
	return(0);
#endif
}
