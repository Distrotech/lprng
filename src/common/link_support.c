/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: link_support.c
 * PURPOSE: open and close data links to a remote host
 **************************************************************************/

static char *const _id =
"$Id: link_support.c,v 3.3 1996/08/25 22:20:05 papowell Exp $";

/***************************************************************************
 * MODULE: Link_support.c
 ***************************************************************************
 * Support for the inter-machine communications
 *
 * int Link_port_num()
 *        gets destination port number for connection
 *
 * int Link_open(char *host, int retry, int timeout )
 *    opens a link to the remote host
 *    if fails, retries 'retry' times at 'interval'
 *    if timeout == 0, wait indefinately
 *    returns socket fd (>= 0); LINK errorcode (negative) if error
 *
 * int Link_listen()
 *  1. opens a socket on the current host
 *  2. set the REUSE option on socket
 *  3. does a bind to port determined by Link_dest_port_num();
 *
 * void Link_close( int socket )
 *    closes the link to the remote host
 *
 * int Link_send( char *host, int *socket,int timeout,
 *    int ch, char *line,int lf,int *ack )
 *    sends 'ch'line'lf' to the remote host
 *    if write/read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    if timeout > 0, i.e.- local, set signal handler
 *    if ch != 0, send ch at start of line
 *    if lf != 0, send LF at end of line
 *    if ack != 0, wait for ack
 *      returns 0 if successful, LINK errorcode if failure
 *      closes socket and sets to LINK errorcode if a failure
 *
 * int Link_ack( char *host, int socket, int timeout, int sendc, int *ack )
 *    if sendc != 0, sends single character sendc;
 *    if ack != 0, wait for ack, and report it
 *    returns 0 if successful, LINK errorcode if failure
 *      closes socket and sets to LINK errorcode if a failure
 *
 * int Link_copy( char *host, int *socket, int timeout,
 *	char *src, int fd, int count)
 *    copies count bytes from fd to the socket
 *    do a timeout on both reading from fd and writing to socket;
 *    if timeout == 0, wait indefinately
 *      returns 0 if successful, LINK errorcode if failure
 *      closes socket and sets to LINK errorcode if a failure
 *
 * int Link_line_read(char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies characters from socket to str until '\n' read,
 *      '\n' NOT copied
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read (less 1 for '\n' )
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    if *count read and not '\n',  then error set; last character
 *       will be discarded.
 *    returns 0 if '\n' read and read <= *count characters
 *            0 if EOF and no characters read (*count == 0)
 *            LINK errorcode otherwise
 *    NOTE: socket is NOT closed on error.
 *
 * int Link_read( char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies '*count' characters from socket to str
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    returns 0 *count not read
 *            LINK errorcode otherwise
 *    socket is closed on error.
 *
 * int Link_file_read( char *host, int *socket, int readtimeout,
 *    int writetimeout, int fd, int *count, int *ack )
 *    reads and copies '*count' characters from socket to fd
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if ack then will read an additional ACK character
 *       returns value in *ack
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    returns 0 *count not read
 *
 ***************************************************************************/

/***************************************************************************
 Commentary:
		Patrick Powell Fri Apr 14 16:43:10 PDT 1995

		These routines have evolved with experience from other systems.
The connection to the remote system involves 4 components:
< local_ip, local_port, remote_ip, remote_port >

Local_ip: There appears to be little reason done to restrict
 a system to originate a connection from a specific IP address.

Local_port:  The BSD UNIX networking software had a concept of reserved
 ports for binding, i.e.- only root could bind (accept on or originate
 connections from) ports 1-1023.  However,  this is now bogus given PCs
 that can use any port.  The requirement is specified in RFC1179, which
 requires origination of connections from 721-731.  Ummm... there is a
 problem with doing this,  as there is also a TCP/IP requirement that
 a port not be reused withing 2*maxLt (10 minutes).  If you spool a
 lot of jobs from a host,  you will have problems.  You have to use
 setsockopt( s, SOL_SOCKET, SO_REUSEADDR, 0, 0 ) to allow reuse of the
 port,  otherwise you may run out of ports to use.

 When using a Unix system,  you can run non-privileged or privileged.
 The remote system will determine if it is going to accept connections
 or not.  Thus,  there is the following possibilities
	1. remote system wants PRIV port, and cannot open them (not root)
	 - hard failure,  need root privs
	2. remote system wants PRIV port, and non available
	 - soft failure,  can retry after timeout
	3. remote system doesnt care, cannot get priv port
	 - try getting unpriv port
	4. remote system doesnt care, cannot get unpriv port
		 - hard failure. Something is very wrong here!

 Configuration options:
   The following options in the configuration file can be used to
   specify control over the local port:

   originate_port (default: blank same as lowportnumber = 0)
   originate_port lowportnumber [highportnumber]
	If lowportnumber is missing or 0, then non-restricted
	ports will be used.
	If lowportnumber is non-zero (true),  and no high port number is
	present,  then a port in the range lowportnumber-1023 will be used.
	If both the lowport and highport and highport numbers
	are present,  a port in the range lowportnumber-highportnumber
	will be used.

Remote_ip: look up address using gethostbyname(), gethostbyaddr()

Remote_port:
	1. check to see if Lpd_port (set by configuratio file) is set
	   - either an integer value or name to look up using getservbyname()
	2. check for "printer" service using getservbyname

 ***************************************************************************/

#include "lp.h"
#include "lp_config.h"
#include "timeout.h"
#include "setuid.h"

static int reserveport (int low, int high, int type );

/***************************************************************************
 * int Link_open(char *host, int retry, int timeout );
 * 1. Set up an inet socket;  a socket has a local host/local port and
 *    remote host/remote port address part.  The routine
 *    will attempt to open a privileged port (number less than
 *    PRIV); if this fails,  it will open a non-privileged port
 * 2. Connect to the remote host on the port specified as follows:
 *    1. Lpd_port - (string) look up with getservbyname("printer","tcp")
 *    2. Lpd_port - (string) convert to integer if possible
 *    3. look up getservbyname("printer","tcp")
 *    Give up with error message
 *
 * In RFC1192, it specifies that the originating ports will be in
 * a specified range; PLP has an option to check that this is so-
 * Allow_non_priv_ports.  Since a reserved port can only
 * be accessed by UID 0 (root) processes, this would appear to prevent
 * ordinary users from directly contacting the remote daemon.
 *
 * However,  enter the PC.  We can generate a connection from ANY port.
 * Thus,  all of these restrictions are somewhat bogus.
 * Note: we use helper functions reserveport() and getconnection()
 ***************************************************************************/

/*
 * reserveport(int max_port_no, min_port_no, type)
 * open a sock and bind to a port, starting at min_port_no, up max_port_no.
 * 
 * NOTE: if min_port_no == maxportno == 0, then we simply open any socket
 *       if min_port_no == 0, maxportno >0 then we open maxportno
 *       if min_port_no < maxportno, then we need a socket in the
 *       specified range.
 * Returns: socket if successful, LINK errorcode if not
 *
 * NOTES AND WARNINGS
 * - This code should work on any system that has a BIND call.
 *   If it does not,  then the reason for failure will not be
 *   bind() call related, but system related.  Watch out for
 *   non-reuse of sockets.  See the setsockopt() call in the code.
 */

int Link_setreuse( int sock )
{
	int option, len;

	len = sizeof( option );
	option = 0;

#ifdef SO_REUSEADDR
	if( getsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, &len ) ){
		logerr_die( LOG_ERR, "Link_setreuse: getsockopt failed" );
	}
	DBGREM4F(DNW4) ("SO_REUSEADDR: socket %d, value %d", sock, option);
	if( option == 0 ){
		option = 1;
		if( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&option, sizeof(option) ) ){
			logerr_die( LOG_ERR, "Link_setreuse: setsockopt failed" );
		}
	}
#endif

	return( option );
}

static int reserveport(int port_no,  int max_port_no, int connection_type )
{
	struct sockaddr_in sin;
	int sock, uid, err;
	int status;

	status = 0;
	DBGREM4F(DNW4) ("reserveport( min %d, max %d )", port_no, max_port_no );
	/* this may or may not be a void function */
	/* we check to see if there is a limit to the reserved ports */

	if( port_no == 0 && max_port_no ){
		port_no = max_port_no;
	}
	/*
	 * if not running SUID root or not root user
	 * check to see if we have less than reserved port
	 */
	if( !UID_root && port_no < IPPORT_RESERVED ){
		port_no = IPPORT_RESERVED;
	}
	if( port_no > max_port_no ){
		port_no = max_port_no = 0;
	}

	/* work your way up */

	status = -1;
	uid = geteuid();
	sock = -1;
	while( status < 0 && port_no <= max_port_no ){
		memset(&sin, 0, sizeof (sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = HostIP;
		sin.sin_port = htons((u_short)port_no);
		/*
		 * do the soket, bind and the set reuse flag operation as
		 * ROOT;  this appears to be the side effect of some
		 * very odd system implementations.  Note that you can
		 * read and write to the socket as a user.
		 */
		if( sock < 0 ){
			if( UID_root ) (void)To_root();
			sock = socket(AF_INET, connection_type, 0);
			err = errno;
			if( UID_root ) (void)To_uid( uid );
			if( sock < 0 ){
				errno = err;
				logerr_die(LOG_DEBUG, "reserveport: socket call failed");
				return( LINK_OPEN_FAIL );
			}
			DBGREM4F(DNW4) ("reserveport: socket %d", sock);
		}
		if( UID_root ) (void)To_root();
		status = bind(sock, (struct sockaddr *)&sin, sizeof(sin));

		/* set up the 'resuse' flag on socket, or you may not be
			able to reuse a port for up to 10 minutes */
		if( status >= 0 ) status = Link_setreuse( sock );

		if( UID_root ) (void)To_uid( uid );

		DBGREM4F(DNW4) ("reserveport: sock %d, port %d, bind and reuse status %d",
			sock, port_no, status );
		if( status >= 0 ){
			break;
		}
		++port_no;
		if( port_no > max_port_no && max_port_no ){
			/* try getting ordinary port as desperation move */
			port_no = max_port_no = 0;
		}
	}

	DBGREM4F(DNW4) ("reserveport: final sock %d, bind status %d, port %d",
		sock, status, port_no);

	if( status < 0 ){
		if( sock >= 0 ) (void) close (sock);
		sock = LINK_OPEN_FAIL;
	}
	return ( sock );
}
/*
 * int getconnection(char *host, int timeout )
 *   opens a connection to the remote host
 * 1. gets the remote host address
 * 2. gets the local port information, and binds a socket
 * 3. makes a connection to the remote host
 */

int getconnection ( char *host, int timeout, int connection_type )
{
	struct hostent *hostent;    /* host entry pointer */
	static int sock;                   /* socket */
	int i, err;                 /* ACME Generic Integers */
	struct sockaddr_in sin;     /* inet socket address */
	int maxportno, minportno;	/* max and minimum port numbers */

	/*
	 * Zero out the sockaddr_in struct
	 */
	sock = -1;
	memset(&sin, 0, sizeof (sin));
	/*
	 * Get the destination host address and remote port number to connect to.
	 */
	DBGREM4F(DNW4)("getconnection: host %s", host);
	sin.sin_family = AF_INET;
	errno = 0;
	if( (hostent = gethostbyname(host)) ){
		/*
		 * set up the address information
		 */
		if( hostent->h_addrtype != AF_INET ){
			logerr(LOG_DEBUG, "getconnection: bad address type for host '%s'",
				host);
			return( LINK_OPEN_FAIL );
		}
		memcpy( &sin.sin_addr, hostent->h_addr, hostent->h_length );
	} else {
		DBGREM3F(DNW3)("getconnection: '%s', trying inet address", host);
		sin.sin_addr.s_addr = inet_addr(host);
		if( sin.sin_addr.s_addr == -1){
			logerr(LOG_DEBUG, "getconnection: unknown host '%s'", host);
			return( LINK_OPEN_FAIL );
		}
	}
	if( Destination_port ){
		sin.sin_port = htons( Destination_port );
	} else {
		sin.sin_port = Link_dest_port_num();
	}
	if( sin.sin_port == 0 ){
		logerr(LOG_INFO,
		"getconnection: bad port number for LPD connection!\n"
		"check 'lpd-port' in configuration file, or the\n"
		"/etc/services file for a missing 'printer 515/tcp' entry" );
		return( LINK_OPEN_FAIL );
	}
	DBGREM3F(DNW3)("getconnection: destination '%s' port %d",
		inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ) );

	/* check on the low and high port values */
	/* we decode the minimum and  maximum range */
	maxportno = minportno = 0;
	if( Originate_port ){
		char *s, *end;
		int m;

		s = end = Originate_port;
		m = strtol( s, &end, 10 );
		if( s != end ){
			maxportno = m;
			s = end;
			m = strtol( s, &end, 10 );
			if( s != end ){
				minportno = m;
			}
		}
	}

	/*
	 * open a socket and bind to a port in the specified range
	 */

	if( maxportno < minportno ){
		i = maxportno;
		maxportno = minportno;
		minportno = i;
	}
	sock = reserveport( minportno, maxportno, connection_type );
	if( sock < 0 ){
		return( sock );
	}

	/*
	 * set up timeout and then make connect call
	 */
	if( Set_timeout( timeout, 0 ) ){
		i = connect (sock, (struct sockaddr *) & sin, sizeof (sin));
	}
	Clear_timeout();
	err = errno;

	DBGREM4F(DNW4)("getconnection: connect sock %d, status %d, errno %d",
		sock, i, err );
	if( i < 0 || Alarm_timed_out ){
		if( Alarm_timed_out ) {
			DBGREM4F(DNW4)("getconnection: connection to '%s' timed out", host);
		} else {
			DBGREM4F(DNW4)("getconnection: connection to '%s' failed '%s'",
				host, Errormsg(err) );
		}
		(void) close (sock);
		sock = LINK_OPEN_FAIL;
	} else {
		struct sockaddr_in src;     /* inet socket address */
		i = sizeof( src );
		if( getsockname( sock, (struct sockaddr *)&src, &i ) < 0 ){
			logerr(LOG_DEBUG,"getconnnection: getsockname failed" );
			(void) close (sock);
			sock = LINK_OPEN_FAIL;
		}
		DEBUG4(
			"getconnection: src ip %s, port %d, dest ip %s, port %d\n",
			inet_ntoa( src.sin_addr ), ntohs( src.sin_port ),
			inet_ntoa( sin.sin_addr ), ntohs( sin.sin_port ) );
	}
	return (sock);
}

/*
 * int Link_listen()
 *  1. opens a socket on the current host
 *  2. set the REUSE option on socket
 *  3. does a bind to port determined by Link_dest_port_num();
 */

int Link_listen()
{
	int sock;                   /* socket */
	int status;                   /* socket */
	struct sockaddr_in sin;     /* inet socket address */
	int euid;					/* euid at time of call*/
	int port;
	int err;

	/*
	 * Zero out the sockaddr_in struct
	 */
	memset(&sin, 0, sizeof (sin));
	/*
	 * Get the destination host address and remote port number to connect to.
	 */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = Link_dest_port_num();
	port = ntohs( sin.sin_port );
	DBGREM4F(DNW4)("Link_listen: lpd port %d", port );

	euid = 0;
	if( UID_root && port < IPPORT_RESERVED && (euid = geteuid()) ){
		(void)To_root();
	}
	errno = 0;
	status = (sock = socket (AF_INET, SOCK_STREAM, 0)) < 0
		|| Link_setreuse( sock ) < 0
		|| bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0;
	err = errno;
	if( euid ) (void)To_uid( euid );
	if( status ){
		DBGREM4F(DNW4)("Link_listen: bind to lpd port %d failed '%s'",
			port, Errormsg(err));
		if( sock >= 0 ){
			(void)close( sock );
			sock = -1;
		}
		errno = err;
		return( LINK_BIND_FAIL );
	}
	status = listen(sock, 10 );	/* backlog of 10 is excessive */
	err = errno;
	if( status ){
		logerr_die(LOG_ERR, "Link_listen: listen failed");
		(void)close( sock );
		err = errno;
		return( LINK_OPEN_FAIL );
	}
	DBGREM4F(DNW4)("Link_listen: port %d, socket %d", ntohs( sin.sin_port ), sock);
	errno = err;
	return (sock);
}

/***************************************************************************
 * int Link_open(char *host, int retry, int timeout );
 ***************************************************************************/

int Link_open(char *host, int retry, int timeout )
{
	int sock;
	DBGREM4F(DNW4) ("Link_open: host '%s', retry %d, timeout %d",host,retry,timeout);
	do {
		sock = getconnection( host, timeout, SOCK_STREAM );
	} while( sock < 0 && retry-- > 0  );
	DBGREM4F(DNW4) ("Link_open: socket %d", sock );
	return( sock );
}

int Link_open_type(char *host, int retry, int timeout, int port, int connection_type )
{
	int sock;
	int oldport = Destination_port;
	Destination_port = port;
	DBGREM4F(DNW4) ("Link_open_type: host '%s', retry %d, timeout %d, port %d, type %d",
		host,retry,timeout, port, connection_type );
	do {
		sock = getconnection( host, timeout, connection_type );
	} while( sock < 0 && retry-- > 0  );
	Destination_port = oldport;
	DBGREM4F(DNW4) ("Link_open_type: socket %d", sock );
	return( sock );
}

/***************************************************************************
 * void Link_close( int socket )
 *    closes the link to the remote host
 ***************************************************************************/

void Link_close( int *sock )
{
	DBGREM4F(DNW4) ("Link_close: closing socket %d", *sock );
	if( *sock >= 0 ){
		(void)close(*sock);
	}
	*sock = -1;
}

int Link_ack( char *host, int *socket, int timeout, int sendc, int *ack )
{
	int status;

	status = Link_send( host, socket, timeout, sendc, (char *)0, 0, ack );
	DBGREM1F(DNW1)("Link_ack: status %d", status );
	return(status);
}


/***************************************************************************
 * int Link_send( char *host, int *socket, int timeout,
 *			 char ch, char *str, int lf, int *ack )
 *    sends 'ch'str to the remote host
 *    if write/read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    if ch != 0, send ch at start of line
 *    if lf != 0, send LF at end of line
 *    if ack != 0, wait for ack, and report it
 *      returns 0 if successful, LINK errorcode if failure
 *      closes socket and sets to LINK errorcode if a failure
 * NOTE: if timeout > 0, local to this function;
 *       if timeout < 0, global to all functions
 *
 * Note: several implementations of LPD expect the line to be written/read
 *     with a single system call (i.e.- TPC/IP does not have the PSH flag
 *     set in the output stream until the last byte, for those in the know)
 *     After having gnashed my teeth and pulled my hair,  I have modified
 *     this code to try to use a single write.  Note that timeouts, errors,
 *     interrupts, etc., may render this impossible on some systems.
 *     Tue Jul 25 05:50:54 PDT 1995 Patrick Powell
 ***************************************************************************/

int Link_send( char *host, int *socket, int timeout,
	int ch, char *send, int lf, int *ack )
{
	static int i, len;      /* Watch out for longjmp * ACME Integers, Inc. */
	char *str;	/* ack value */
	static int status;		/* return status */
	static int msg_length;
	static char *buffer;	/* buffer */
	static int max_str;	/* maximum buffer length */
	int err = 0;

	/*
	 * set up initial conditions
	 */
	len = i = status = 0;	/* shut up GCC */
	if(*socket < 0) {
		DBGREM1F(DNW1)( "Link_send: bad socket" );
		return( LINK_TRANSFER_FAIL );
	}
	if( ack ){
		*ack = 0;
	}

	/* get total message length */
	msg_length = 0;
	if( ch ) ++msg_length;
	if( send ) msg_length += strlen( send );
	if( lf ) ++msg_length;

	/* allocate buffer if necessary */
	if( buffer == 0 || max_str <= msg_length ){
		if( buffer ){
			free(buffer);
			buffer = 0;
		}
		max_str += msg_length + 256;	/* lengthen it */
		malloc_or_die( buffer, max_str );
	}

	/* set up the string to send */
	str = buffer;
	if( ch ) *str++ = ch;
	if( send ){
		strcpy( str, send );
		str += strlen( send );
	}
	if( lf ) *str++ = lf;
	/* make sure strings are terminated */
	*str++ = 0;

	DBGREM1F(DNW1)( "Link_send: host '%s' socket %d, timeout %d",
		host, *socket, timeout );
	DBGREM1F(DNW1)( "Link_send: ch 0x%x, str '%s', lf  0x%x, ack 0x%x",
		ch, send, lf, ack );
	DBGREM8F(DNW5)("Link_send: write len %d, '%d'%s'",
		msg_length, buffer[0], &buffer[1] );

	/*
	 * set up timeout and then write
	 */
	
	if( Set_timeout( timeout, socket ) ){
		i = Write_fd_len( *socket, buffer, msg_length );
	}
	Clear_timeout();
	err = errno;

	/* now decode the results */
	DBGREM3F(DNW3)("Link_send: final write status %d", i );
	if( i < 0 || Alarm_timed_out ){
		if( Alarm_timed_out ){
			DEBUG8("Link_send: write to '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else {
			DEBUG8("Link_send: write to '%s' failed '%s'",
				host, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
		}
	}

	/* check for an ACK to be received */
	if( status == 0 && ack ){
		DBGREM3F(DNW3)("Link_send: ack required" );
		buffer[0] = 0;
		len = 1;	/* special case, only 1 char! */
		while( len > 0 ){
			if( Set_timeout( timeout, socket ) ){
				i = read (*socket, buffer, len );
			}
			Clear_timeout();
			err = errno;

			if( Alarm_timed_out || i <= 0 ) break;
			len -= i;
		}
		*ack = buffer[0];
		if( i <= 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DBGREM3F(DNW3)("Link_send: ack read from '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
				DBGREM3F(DNW3)("Link_send: ack read from '%s' failed - %s",
					host, Errormsg(err) );
				status = LINK_TRANSFER_FAIL;
			}
		} else if( *ack ){
			status = LINK_ACK_FAIL;
		}
		DBGREM3F(DNW3)("Link_send: ack read status %d, operation status %s, ack=%s",
			i, Link_err_str(status), Ack_err_str(*ack) );
		if( status == 0 ){
			/* check to see if you have some additional stuff pending */
			fd_set readfds;
			struct timeval delay;

			memset( &delay,0,sizeof(delay));
			FD_ZERO( &readfds );
			FD_SET( *socket, &readfds );
			i = select( (*socket)+1,(fd_set *)&readfds,
				(fd_set *)0, (fd_set *)0, &delay );
			if( i != 0 ){
				log( LOG_ERR,
				"Link_send: PROTOCOL ERROR - pending input from '%s' after ACK received",
				host );
			}
		}
	}
	if( status && status != LINK_ACK_FAIL ){
		Link_close( socket );
	}
	DBGREM1F(DNW1)("Link_send: final status %s", Link_err_str(status) );
	return (status);
}

/***************************************************************************
 * int Link_copy( char *host, int *socket, int timeout,
 *  char *src, int fd, int count)
 *    copies count bytes from fd to the socket
 *    if write does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    if count < 0, will read until end of file
 *      returns 0 if successful, LINK errorcode if failure
 ***************************************************************************/
int Link_copy( char *host, int *socket, int readtimeout, int writetimeout,
	char *src, int fd, int pcount)
{
	char buf[LARGEBUFFER];      /* buffer */
	static int len;              /* ACME Integer, Inc. */
	static int status;				/* status of operation */
	static int count;	/* might be clobbered by longjmp */
	int err;					/* saved error status */

	count = pcount;
	len = status = 0;	/* shut up GCC */
	DBGREM4F(DNW4)("Link_copy: sending %d of '%s' to %s, rdtmo %d, wrtmo %d, fd %d",
		count, src, host, readtimeout, writetimeout, fd );
	/* check for valid socket */
	if(*socket < 0) {
		DBGREM4F(DNW4)( "Link_copy: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/* do the read */
	while( status == 0 && count != 0 ){
		len = count;
		if( count < 0 || len > sizeof(buf) ) len = sizeof(buf);
		/* do the read with timeout */
		if( Set_timeout( readtimeout, socket ) ){
			len = read( fd, buf, len );
		}
		Clear_timeout();
		err = errno;

		if( Alarm_timed_out || len <= 0 ){
			/* EOF on input */
			if( count > 0 ){
				DBGREM4F(DNW4)(
					"Link_copy: read from '%s' failed, %d bytes left - %s",
					src, count, Errormsg(err) );
				status = LINK_TRANSFER_FAIL;
			} else {
				DBGREM4F(DNW4)("Link_copy: read status %d count %d", len, count );
				status = 0;
			}
			break;
		}
		if( count > 0 ){
			count -= len;
		}

		DBGREM4F(DNW4)("Link_copy: read %d bytes", len );
		if( Set_timeout( writetimeout, socket ) ){
			len = Write_fd_len( *socket, buf, len );
		}
		Clear_timeout();
		err = errno;

		DBGREM4F(DNW4)("Link_copy: write done, status %d", len );
		if( len < 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DBGREM4F(DNW4)("Link_copy: write to '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
				DBGREM4F(DNW4)("Link_copy: write to '%s' failed - %s",
					host, Errormsg(err) );
				status = LINK_TRANSFER_FAIL;
			}
		}
	}
	
	if( status == 0 ){
		/* check to see if you have some additional stuff pending */
		fd_set readfds;
		struct timeval delay;
		int i;

		memset( &delay,0,sizeof(delay));
		FD_ZERO( &readfds );
		FD_SET( *socket, &readfds );
		i = select( *socket+1,(fd_set *)&readfds,
			(fd_set *)0, (fd_set *)0, &delay );
		if( i != 0 ){
			log( LOG_ERR,
			"Link_copy: PROTOCOL ERROR - pending input from '%s' after transfer",
			host );
		}
	}
	DBGREM4F(DNW4)("Link_copy: status %d", status );
	return( status );
}


/***************************************************************************
 * Link_dest_port_num () {
 * Get the destination port number
 * look up the service in the service directory using getservent
 ***************************************************************************/
int Link_dest_port_num()
{
	struct servent *sp;
	char *s;
	int port_num;

	s = Lpd_port;
	if( s == 0 ){
		s = "printer";
	}
	port_num = 0;

	if ((sp = getservbyname(s, "tcp")) == 0) {
		DBGREM4F(DNW4)("getservbyname(\"%s\",tcp) failed", s);
		/* try integer value */
		port_num = htons( atoi( s ) );
	} else {
		port_num = sp->s_port;
	}
	DBGREM4F(DNW4)("Link_dest_port_num: port %s = %d", s, ntohs( port_num ) );
	return (port_num);
}

/***************************************************************************
 * int Link_line_read(char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies characters from socket to str until '\n' read,
 *      '\n' NOT copied
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read (less 1 for '\n' )
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    if *count read and not '\n',  then error set; last character
 *       will be discarded.
 *    returns 0 if '\n' read at or before *count characters
 *            0 if EOF and no characters read (*count == 0)
 *            LINK errorcode otherwise
 *    socket is closed on error.
 ***************************************************************************/

int Link_line_read(char *host, int *socket, int timeout,
	  char *buf, int *count )
{
	static int i, len, max, err = 0;	/* ACME Integer, Inc. */
	static int status;				/* status of operation */
	static char *s = 0;				/* location of \n */

	len = i = status = 0;	/* shut up GCC */
	DBGREM4F(DNW4) ("Link_line_read: reading %d from '%s' on %d, timeout %d",
		*count, host, *socket, timeout );
	/* check for valid socket */
	if(*socket < 0) {
		DBGREM4F(DNW4)("Link_line_read: bad socket" );
		*count = 0;
		return (LINK_OPEN_FAIL);
	}
	/* do the read */
	buf[0] = 0;
	len = 0;
	max = *count;
	if( max > 0 ){
		--max;
		buf[ max ] = 0;
	}
	/*
	 * set up timeout and then do operation
	 */
	s = 0;
	while( len < max ){
		if( Set_timeout( timeout, socket ) ){
			i = read( *socket, &buf[len], max - len );
		}
		Clear_timeout();
		err = errno;
		if( i <= 0 || Alarm_timed_out ){
			break;
		}
		len += i;
		buf[len] = 0;
		DBGREM4F(DNW4)("Link_line_read: len %d, last read %d, last '0x%02x' '%s'",
			len, i, buf[len-1], buf );
		if( (s = strchr( buf, '\n' )) ){
			break;
		}
	}
	/*
	 * conditions will be:
	 * long line, timeout, error, or OK
	 */
	if( Alarm_timed_out ){
		DBGREM4F(DNW4)( "Link_line_read: read from '%s' timed out", host);
		status = LINK_TRANSFER_FAIL;
	} else if( i == 0 ){
		DBGREM4F(DNW4)("Link_line_read: EOF from '%s'", host );
		status = LINK_TRANSFER_FAIL;
	} else if( i < 0 ){
		DBGREM4F(DNW4)("Link_line_read: read from '%s' failed - %s", host,
			Errormsg(err) );
		status = LINK_TRANSFER_FAIL;
	} else if( s == 0 ){
		DBGREM4F(DNW4)("Link_line_read: no LF on line from '%s'", host );
		status = LINK_LONG_LINE_FAIL;
	} else {
		*s++ = 0;
		DBGREM4F(DNW4)("Link_line_read: len %d, LF '%d'", len, s-buf );
		/* get the length of line */
		i  = len - (s - buf);
		if( i ){
			log( LOG_ERR,
				"Link_line_read: PROTOCOL ERROR - '%d' chars after newline",
				i );
		}
	}
	*count = len;

	if( status ){
		Link_close( socket );
	}
	if( status == 0 ){
		/* check to see if you have some additional stuff pending */
		fd_set readfds;
		struct timeval delay;

		memset( &delay,0,sizeof(delay));
		FD_ZERO( &readfds );
		FD_SET( *socket, &readfds );
		i = select( *socket+1,(fd_set *)&readfds,
			(fd_set *)0, (fd_set *)0, &delay );
		if( i != 0 ){
			log( LOG_ERR,
			"Link_line_read: PROTOCOL ERROR - pending input from '%s' after transfer",
			host );
		}
	}

	DBGREM4F(DNW4)("Link_line_read: status %d", status );
	errno = err;
	return( status );
}


/***************************************************************************
 * int Link_read(char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies '*count' characters from socket to str
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    returns 0 *count not read
 *        LINK errorcode otherwise
 *    socket is closed on error.
 ***************************************************************************/

int Link_read(char *host, int *socket, int timeout,
	  char *buf, int *count )
{
	static int i, len;              /* ACME Integer, Inc. */
	static char *str;					/* input buffer pointer */
	static int status;				/* status of operation */
	int err;

	err = len = i = status = 0;	/* shut up GCC */
	DBGREM4F(DNW4) ("Link_read: reading %d from '%s' on socket %d",
		*count, host, *socket );
	/* check for valid socket */
	if(*socket < 0) {
		DBGREM4F(DNW4)( "Link_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	errno = 0;
	/* do the read */
	if( *count < 0 ) *count = 0;
	len = *count;
	str = buf;
	while( len > 0 ){
		/*
		 * set up timeout and then do operation
		 */
		if( Set_timeout( timeout, socket ) ){
			i = read( *socket, str, len );
		}
		Clear_timeout();
		err = errno;
		DBGREM9F(DNW6)("Link_read: len %d read %d", len, i );
		if( i <= 0 || Alarm_timed_out ) break;
		len -= i;
		str += i;
	}
	*count = *count - len;

	if( (*count == 0) ){
		if( Alarm_timed_out ){
			DBGREM4F(DNW4)("Link_read: read %d from '%s' timed out", len, host, i );
		} else if( i < 0 ) {
			DBGREM4F(DNW4)("Link_read: read %d from '%s' failed - %s",
				len, host, i, Errormsg(err) );
		}
		status = LINK_TRANSFER_FAIL;
	}
	
	if( status ){
		Link_close( socket );
	}

	DBGREM4F(DNW4)("Link_read: status %d", status );
	errno = err;
	return( status );
}

/***************************************************************************
 * int Link_file_read( char *host, int *socket, int readtimeout,
 *    int writetimeout, int fd, int *count, int *ack )
 *    reads and copies '*count' characters from socket to fd
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if ack then will read an additional ACK character
 *       returns value in *ack
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinately
 *    returns 0 *count not read
 *
 ***************************************************************************/

int Link_file_read(char *host, int *socket, int readtimeout, int writetimeout,
	  int fd, int *count, int *ack )
{
	char str[LARGEBUFFER];		/* input buffer pointer */
	static int i, l, len, cnt;			/* number to read or write */
	static int status;				/* status of operation */
	int err;					/* error */

	len = i = status = cnt = 0;	/* shut up GCC */
	DBGREM4F(DNW4) ("Link_file_read: reading %d from '%s' on %d",
		*count, host, *socket );
	/* check for valid socket */
	if(*socket < 0) {
		DBGREM4F(DNW4)( "Link_file_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do the transfer
	 */

	/* do the read */
	len = *count;
	while( status == 0 && len > 0 ){
		l = sizeof(str);
		if( l > len ) l = len;
		if( Set_timeout( readtimeout, socket ) ){
			i = read( *socket, str, l );
		}
		Clear_timeout();
		err = errno;
		if( Alarm_timed_out ){
			DBGREM4F(DNW4)( "Link_file_read: read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DBGREM9F(DNW6)("Link_file_read: len %d, readlen %d, read %d", len, l, i );
			len -= i;

			if( Set_timeout( writetimeout, 0 ) ){
				cnt = Write_fd_len( fd, str, i );
			}
			Clear_timeout();
			err = errno;

			if( Alarm_timed_out || cnt < 0 ){
				DBGREM4F(DNW4)( "Link_file_read: write %d to fd %d failed - %s",
					i, fd, Errormsg(err) );
				status = LINK_TRANSFER_FAIL; 
			}
		} else {
			DBGREM4F(DNW4)("Link_file_read: read from '%s' failed - %s",
				host, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
		}
	}
	*count -= len;

	if( status == 0 && ack ){
		DBGREM9F(DNW6)("Link_file_read: doing ACK read" );
		if( Set_timeout( readtimeout, socket ) ){
			i = read( *socket, str, 1 );
		}
		Clear_timeout();
		err = errno;

		if( Alarm_timed_out ){
			DBGREM4F(DNW4)( "Link_file_read: ack read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DBGREM9F(DNW6)("Link_file_read: ACK read count %d value %d", i, *str );
			*ack = *str;
			if( *ack ){
				DBGREM2F(DNW2)( "Link_file_read: nonzero ack '%d'", *ack );
				status = LINK_ACK_FAIL;
			}
		} else {
			DBGREM4F(DNW4)("Link_file_read: ack read from '%s' failed - %s",
				len, host, i, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
			DBGREM2F(DNW2)( "Link_file_read: reading ack failed" );
			status = LINK_TRANSFER_FAIL;
		}
		if( status == 0 ){
			/* check to see if you have some additional stuff pending */
			fd_set readfds;
			struct timeval delay;

			memset( &delay,0,sizeof(delay));
			FD_ZERO( &readfds );
			FD_SET( *socket, &readfds );
			i = select( *socket+1,(fd_set *)&readfds,
				(fd_set *)0, (fd_set *)0, &delay );
			if( i != 0 ){
				log( LOG_ERR,
				"Link_file_read: PROTOCOL ERROR - pending input from '%s' after transfer",
				host );
			}
		}
	}
	if( status ){
		Link_close( socket );
	}

	DBGREM4F(DNW4)("Link_file_read: status %d", status );
	return( status );
}

#define PAIR(X) { #X, X }

static struct link_err {
    char *str;
    int value;
} link_err[] = {
{ "NO ERROR", 0 },
PAIR(LINK_OPEN_FAIL),
PAIR(LINK_TRANSFER_FAIL),
PAIR(LINK_ACK_FAIL),
PAIR(LINK_FILE_READ_FAIL),
PAIR(LINK_LONG_LINE_FAIL),
PAIR(LINK_BIND_FAIL),
PAIR(LINK_PERM_FAIL),
{0,0}
};

const char *Link_err_str (int n)
{
    int i;
    static char buf[40];
	const char *s = 0;

	for( i = 0; link_err[i].str && link_err[i].value != n; ++i );
	s = link_err[i].str;
	if( s == 0 ){
		s = buf;
		(void) plp_snprintf (buf, sizeof(buf), "link error %d", n);
	}
    return(s);
}

static struct link_err ack_err[] = {
PAIR(ACK_SUCCESS),
PAIR(ACK_STOP_Q),
PAIR(ACK_RETRY),
PAIR(ACK_FAIL),
PAIR(LINK_ACK_FAIL),
{0,0}
};


const char *Ack_err_str (int n)
{
    int i;
    static char buf[40];
	const char *s = 0;

	for( i = 0; ack_err[i].str && ack_err[i].value != n; ++i );
	s = ack_err[i].str;
	if( s == 0 ){
		s = buf;
		(void) plp_snprintf (buf, sizeof(buf), "ack error %d", n);
	}
    return(s);
}
