/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: monitor.c
 * PURPOSE: get status information sent from server
 **************************************************************************/

static char *const _id =
"monitor.c,v 3.6 1998/03/29 18:32:46 papowell Exp";

#include "lp.h"
/**** ENDINCLUDE ****/

/*
 * Monitor for TCP/UDP data
 *  Opens a UDP or TCP socket and waits for data to be sent to it.
 *
 *  monitor [-t] [-u] [port]
 *   port is an integer number or a service name in the services database
 *   default is to use UDP.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "portable.h"

# else

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

#endif


extern int errno;

int udp_open( int port );
int tcp_open( int port );

extern int Link_setreuse( int sock );
const char * Errormsg ( int err );

char buffer[10240];
int use_tcp;
int use_udp;
int port_num = 2001;
int udp_fd = -1;
int tcp_fd = -1;

char *prog = "???";

/*****************************************************************
 * Command line options and Debugging information
 * Getopt is a modified version of the standard getopt(3) command
 *  line parsing routine. See getopt.c for details
 *****************************************************************/

/* use this before any error printing, sets up program Name */
int Getopt( int argc, char *argv[], char *optstring );
extern int Optind, Opterr;

void usage(void)
{
	char *s;

	if( (s = strrchr( prog, '/')) ){
		prog  = s+1;
	}
	fprintf( stderr, "usage: %s [-u] [-t] [port]\n", prog );
	fprintf( stderr, "  -u = use UDP\n" );
	fprintf( stderr, "  -t = use TCP (default)\n" );
	fprintf( stderr, "  port = port to use (%d default)\n", port_num  );
	exit(1);
}
	
int count_fd;
fd_set readfds;
fd_set testfds;


int main(int argc, char *argv[] )
{
	int n, i, c, err;
	char *portname;
	struct servent *servent;
	prog = argv[0];
	while( (n = Getopt(argc, argv, "ut")) != EOF ){
		switch(n){
		default: usage(); break;
		case 'u': use_udp = 1; break;
		case 't': use_tcp = 1; break;
		}
	}
	i = argc - Optind;
	if( i > 1 ) usage();
	if( i == 1 ){
		portname = argv[Optind];
		n = atoi( portname );
		if( n <= 0 ){
			servent = getservbyname( portname, "udp" );
			if( servent ){
				n = ntohs( servent->s_port );
			}
		}
		if( n <= 0 ){
			fprintf( stderr, "udp_open: bad port number '%s'\n",portname );
			usage();
		}
		port_num = n;
	}

	if( !use_tcp && !use_udp ) use_udp = 1;

	if( use_udp ){
		udp_fd = udp_open( port_num );
	}
	if( use_tcp ){
		tcp_fd = tcp_open( port_num );
	}

	FD_ZERO( &readfds );

	if( udp_fd > 0 ){
		FD_SET(udp_fd, &readfds);
		if( udp_fd >= count_fd ) count_fd = udp_fd + 1;
	}
	if( tcp_fd > 0 ){
		FD_SET(tcp_fd, &readfds);
		if( tcp_fd >= count_fd ) count_fd = tcp_fd + 1;
	}
	while(1){
		testfds = readfds;
		n = select( count_fd,
			FD_SET_FIX((fd_set *))&testfds,
			FD_SET_FIX((fd_set *))0, FD_SET_FIX((fd_set *))0,
			(struct timeval *)0 );
		err = errno;
		if( n < 0 ){
			fprintf( stderr, "select error - %s\n", Errormsg(errno) );
			if( err != EINTR ) break;
		}
		for( i = 0; n > 0 ; ++i ){
			if( FD_ISSET(i, &testfds) ){
				--n;
				if( i == tcp_fd ){
					struct sockaddr_in sin;
					int len = sizeof( sin );
					i = accept( tcp_fd, (struct sockaddr *)&sin, &len );
					if( i < 0 ){
						fprintf( stderr, "accept error - %s\n",
							Errormsg(errno) );
						continue;
					}
					fprintf( stdout, "connection from %s\n",
						inet_ntoa( sin.sin_addr ) );
					if( i >= count_fd ) count_fd = i + 1;
					FD_SET(i, &readfds);
				} else {
					c = read( i, buffer, sizeof(buffer)-1 );
					if( c == 0 ){
						/* closed connection */
						fprintf(stdout, "closed connection %d\n", i );
						if( i != udp_fd ){
							close( i );
							FD_CLR(i, &readfds );
						}
					} else if( c > 0 ){
						buffer[c] = 0;
						fprintf( stdout, "recv port %d: %s\n", i, buffer );
						fflush(stdout);
					} else {
						fprintf( stderr, "read error - %s\n",
							Errormsg(errno) );
						close( i );
						FD_CLR(i, &readfds );
					}
				}
			}
		}
	}
	return(0);
}

int udp_open( int port )
{
	int i, fd, err;
	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_DGRAM, 0 );
	err = errno;
	if( fd < 0 ){
		fprintf(stderr,"udp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = -1;
	i = bind( fd, (struct sockaddr *) & sin, sizeof (sin) );
	err = errno;

	if( i < 0 ){
		fprintf(stderr,"udp_open: bind to '%s port %d' failed - %s\n",
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ),
			Errormsg(errno) );
		close(fd);
		fd = -1;
	}
	if( fd == 0 ){
		fd = dup(fd);
		if( fd < 0 ){
			fprintf(stderr,"udp_open: dup failed - %s\n",
				Errormsg(errno) );
			close(fd);
			fd = -1;
		}
	}
	return( fd );
}


int tcp_open( int port )
{
	int i, fd, err;
	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_STREAM, 0 );
	err = errno;
	if( fd < 0 ){
		fprintf(stderr,"tcp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = bind( fd, (struct sockaddr *) & sin, sizeof (sin) );
	if( i >= 0 ) i = Link_setreuse( fd );
	if( i >= 0 ) i = listen( fd, 10 );
	err = errno;

	if( i < 0 ){
		fprintf(stderr,"tcp_open: connect to '%s port %d' failed - %s\n",
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ),
			Errormsg(errno) );
		close(fd);
		fd = -1;
	}
	if( fd == 0 ){
		fd = dup(fd);
		if( fd < 0 ){
			fprintf(stderr,"tcp_open: dup failed - %s\n",
				Errormsg(errno) );
			close(fd);
			fd = -1;
		}
	}
	return( fd );
}


/****************************************************************************
 * Extract the necessary definitions for error message reporting
 ****************************************************************************/

#if !defined(HAVE_STRERROR)
# if defined(HAVE_SYS_NERR)
#   if !defined(HAVE_SYS_NERR_DEF)
      extern int sys_nerr;
#   endif
#   define num_errors    (sys_nerr)
# else
#  	define num_errors    (-1)            /* always use "errno=%d" */
# endif
# if defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_SYS_ERRLIST_DEF)
    extern const char *const sys_errlist[];
#  endif
# else
#  undef  num_errors
#  define num_errors   (-1)            /* always use "errno=%d" */
# endif
#endif

const char * Errormsg ( int err )
{
    const char *cp;

#if defined(HAVE_STRERROR)
	cp = strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err <= num_errors) {
		cp = sys_errlist[err];
    } else
# endif
	{
		static char msgbuf[32];     /* holds "errno=%d". */
		/* SAFE use of sprintf */
		(void) sprintf (msgbuf, "errno=%d", err);
		cp = msgbuf;
    }
#endif
    return (cp);
}

int Link_setreuse( int sock )
{
	int option, len;

	len = sizeof( option );
	option = 0;

#ifdef SO_REUSEADDR
	if( getsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, &len ) ){
		fprintf( stderr, "Link_setreuse: getsockopt failed" );
		exit(1);
	}
	if( option == 0 ){
		option = 1;
		if( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&option, sizeof(option) ) ){
			fprintf( stderr, "Link_setreuse: setsockopt failed" );
		}
	}
#endif

	return( option );
}
