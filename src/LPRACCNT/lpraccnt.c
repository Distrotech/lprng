/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpraccnt.c
 * PURPOSE: do accounting
 **************************************************************************/

static char *const _id =
"lpraccnt.c,v 3.3 1997/09/18 19:45:50 papowell Exp";
/*
 * Monitor for Accounting Information
 *  Opens a tcp socket and waits for data to be sent to it.
 *
 *  lpdaccnt [-Ddebug] [port]
 *   port is an integer number or a service name in the services database
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "portable.h"
#endif

#include "lp.h"
/**** ENDINCLUDE ****/

int tcp_open( char *portname );

char buffer[1024];
char *portname = "3000";

const char * Errormsg ( int err );

extern int Getopt(), Optind;
extern char *Optarg;
char *prog = "???";

void usage()
{
	fprintf( stderr, "usage: %s [-D debug] [-p port]\n", prog );
	exit(1);
}

int main( int argc, char *argv[] )
{
	int fd2, fd1;
	int fds, maxfds, avail, n, i, cnt;
	fd_set readfds;
	fd_set exceptfds;
	fd_set use_readfds;
	fd_set use_exceptfds;
	struct sockaddr addr;
	int len;

	if( argv[0] ) prog = argv[0];
	while( (i = Getopt( argc, argv, "D:p:")) != EOF ){
		switch( i ){
		default: usage(); break;
		case 'D':
#if !defined(NODEBUG)
			Debug = atoi( Optarg );
#endif
			break;
		case 'p': portname = Optarg; break;
		}
	}
	if( Optind < argc ) usage();
	fd2 = tcp_open( portname );
	if( fd2 < 0 ){
		usage();
	}
	maxfds = fd2+1;
	if(DEBUGL0)fprintf(stdout,"fd2 %d\n", fd2 );
	FD_ZERO( &readfds );
	FD_ZERO( &exceptfds );
	FD_SET( fd2, &readfds );
	FD_SET( fd2, &exceptfds );
	while(1){
		memcpy( &use_readfds, &readfds, sizeof(use_readfds) );
		memcpy( &use_exceptfds, &exceptfds, sizeof(use_exceptfds) );
		fds = maxfds;
		fd1 = 0;
		avail = select( fds,
			FD_SET_FIX((fd_set *))&use_readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))&use_exceptfds, (struct timeval *)0 );
		n = errno;
		if(DEBUGL0)fprintf(stdout,"select returned %d\n", avail);
		if( avail == -1 && n != EINTR ) break;
		for(; avail > 0; --avail ){
			/* find out if a read request is outstanding */
			if(DEBUGL0)fprintf(stdout,"checking, avail %d, starting at %d\n", avail, fd1);
			for( ; fd1 < fds
				&& !FD_ISSET( fd1, &use_readfds)
				&& !FD_ISSET( fd1, &use_exceptfds); ++fd1 );
			if(DEBUGL0)fprintf(stdout,"found fd %d\n", fd1);
			if( fd1 >= fds ){
				fprintf( stderr, "%s: bad select! '%s'\n", prog, Errormsg(errno) );
				exit(1);
			}
			FD_CLR( fd1, &use_readfds );
			FD_CLR( fd1, &use_exceptfds );
			if( fd1 != fd2 ){
				n = read( fd1, buffer, sizeof(buffer)-1);
				if( n == 0 ){
					/* we have an EOF condition */
					close( fd1 );
					FD_CLR( fd1, &readfds );
					FD_CLR( fd1, &exceptfds );
				} else {
					if( buffer[n-1] != '\n' ){
						buffer[n] = '\n';
						++n;
					}
					for( i = 0; i < n; i += cnt ){
						cnt = write(1, buffer+i, n-i );
						if( cnt < 0 ){
							fprintf( stderr, "%s: bad write! '%s'\n", prog,
								Errormsg( errno ) );
							exit(1);
						}
					}
				}
			} else {
				len = sizeof(addr);
				fd1 = accept( fd2, &addr, &len );
				if( fd1 < 0 ){
					fprintf( stderr, "accept failed - '%s'", Errormsg(errno) );
					continue;
				}
				len = 1;
				if(DEBUGL0)fprintf(stdout, "accepted %d\n", fd1 );
				/* now put it into the list */
				FD_SET( fd1, &readfds );
				FD_SET( fd1, &exceptfds );
				if( fd1 >= maxfds ) maxfds = fd1+1;
			}
		}
	}
	return(0);
}

int tcp_open( char *portname )
{
	int port, i, fd, err, len;
	struct sockaddr_in sin;
	struct servent *servent;

	port = atoi( portname );
	if( port <= 0 ){
		servent = getservbyname( portname, "tcp" );
		if( servent ){
			port = ntohs( servent->s_port );
		}
	}
	if( port <= 0 ){
		fprintf( stderr, "tcp_open: bad port number '%s'\n",portname );
		return( -1 );
	}
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_STREAM, 0 );
	err = errno;
	if( fd < 0 ){
		fprintf(stderr,"tcp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = -1;
	i = bind( fd, (struct sockaddr *) & sin, sizeof (sin) );
	len = 1;
	setsockopt( fd, SOL_SOCKET, SO_REUSEADDR,
		(char *)&len, sizeof( len ) );
	if( i != -1 ) i = listen( fd, 10 );
	err = errno;

	if( i < 0 ){
		fprintf(stderr,"tcp_open: connect to '%s port %d' failed - %s\n",
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ),
			Errormsg(errno) );
		close(fd);
		fd = -1;
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
