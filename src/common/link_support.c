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
"$Id: link_support.c,v 3.5 1997/02/04 21:41:05 papowell Exp papowell $";

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

 ***************************************************************************/

#include "lp.h"
#include "linksupport.h"
#include "setuid.h"
#include "timeout.h"
#include "gethostinfo.h"
/**** ENDINCLUDE ****/

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
 * a specified range, 721-731; Since a reserved port can only
 * be accessed by UID 0 (root) processes, this would appear to prevent
 * ordinary users from directly contacting the remote daemon.
 * However,  enter the PC.  We can generate a connection from ANY port.
 * Thus,  all of these restrictions are somewhat bogus.  We use the
 * configuration file 'PORT' restrictions instead
 ***************************************************************************/

int AF_Protocol = AF_INET;

int Link_setreuse( int sock )
{
	int option, len;

	len = sizeof( option );
	option = 0;

#ifdef SO_REUSEADDR
	if( getsockopt( sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, &len ) ){
		logerr_die( LOG_ERR, "Link_setreuse: getsockopt failed" );
	}
	DEBUGF(DNW4) ("SO_REUSEADDR: socket %d, value %d", sock, option);
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
/*
 * int getconnection(char *host, int timeout )
 *   opens a connection to the remote host
 * 1. gets the remote host address
 * 2. gets the local port information, and binds a socket
 * 3. makes a connection to the remote host
 */


static int connect_timeout( int timeout,
	int sock, struct sockaddr *name, int namelen)
{
	int status = -1;
	int err = 0;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout, 0);
		status = connect(sock, name, namelen );
		err = errno;
	} else {
		status = -1;
		err = errno;
	}
	Clear_timeout();
	errno = err;
	return( status );
}

int getconnection ( char *hostname, int timeout, int connection_type )
{
	int sock;            /* socket */
	int i, err;            /* ACME Generic Integers */
	struct sockaddr_in dest_sin;     /* inet socket address */
	struct sockaddr_in src_sin;     /* inet socket address */
	int maxportno, minportno;	/* max and minimum port numbers */
	int euid;
	int status;				/* status of operation */
	plp_block_mask oblock;

	/*
	 * find the address
	 */
	DEBUGF(DNW1)("getconnection: host %s, timeout %d, connection_type %d",
		hostname, timeout, connection_type);
	euid = geteuid();
	sock = -1;
	memset(&dest_sin, 0, sizeof (dest_sin));
	dest_sin.sin_family = AF_Protocol;
	if( Find_fqdn( &LookupHostIP, hostname, 0 ) ){
		/*
		 * Get the destination host address and remote port number to connect to.
		 */
		DEBUGF(DNW1)("getconnection: fqdn %s", LookupHostIP.fqdn);
		dest_sin.sin_family = LookupHostIP.host_addrtype;
		if( LookupHostIP.host_addrlength > sizeof( dest_sin.sin_addr ) ){
			fatal( LOG_ALERT, "getconnection: addresslength outsize value");
		}
		memcpy( &dest_sin.sin_addr, (void *)LookupHostIP.host_addr_list.list,
			LookupHostIP.host_addrlength );
	} else if( inet_pton( AF_Protocol, hostname, &dest_sin.sin_addr ) != 1 ){
		DEBUGF(DNW2)("getconnection: cannot get address for '%s'", hostname );
		return( LINK_OPEN_FAIL );
	}
	if( Destination_port ){
		dest_sin.sin_port = htons( Destination_port );
	} else {
		dest_sin.sin_port = Link_dest_port_num();
	}
	if( dest_sin.sin_port == 0 ){
		logerr(LOG_INFO,
		"getconnection: bad port number for LPD connection!\n"
		"check 'lpd-port' in configuration file, or the\n"
		"/etc/services file for a missing 'printer 515/tcp' entry" );
		return( LINK_OPEN_FAIL );
	}
	DEBUGF(DNW2)("getconnection: destination IP '%s' port %d",
		inet_ntoa( dest_sin.sin_addr ), ntohs( dest_sin.sin_port ) );

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
	/* check for reversed order ... */
	if( maxportno < minportno ){
		i = maxportno;
		maxportno = minportno;
		minportno = i;
	}
	/* check for only one */
	if( minportno == 0 ){
		minportno = maxportno;
	}
	/*
	 * if not running SUID root or not root user
	 * check to see if we have less than reserved port
	 */
	if( !UID_root && minportno < IPPORT_RESERVED ){
		minportno = IPPORT_RESERVED;
		if( minportno > maxportno ){
			minportno = maxportno = 0;
		}
	}
	DEBUGF(DNW2)("getconnection: minportno %d, maxportno %d",
		minportno, maxportno );

	/* we now have a range of ports and a starting position
	 * Note 1: if port == 0, then we get any port,
	 *  and do not set SOCKREUSE.  We only try once.
     * Note 2: if port != 0, then we get port in range.
     *  we DO set SOCKREUSE.
     * Note 3. we try all ports in range; if none work,
     *  we get an error.
     */ 
	do{
		/*
		 * do the sock et, bind and the set reuse flag operation as
		 * ROOT;  this appears to be the side effect of some
		 * very odd system implementations.  Note that you can
		 * read and write to the socket as a user.
		 */
		plp_block_all_signals( &oblock );
		if( UID_root ) (void)To_root();
		sock = socket(AF_Protocol, connection_type, 0);
		err = errno;
		if( UID_root ) (void)To_uid( euid );
		plp_unblock_all_signals( &oblock );
		if( sock < 0 ){
			errno = err;
			logerr_die(LOG_DEBUG, "getconnection: socket call failed");
		}
		DEBUGF(DNW4) ("getconnection: socket %d", sock);

		src_sin.sin_family = AF_Protocol;
		/* src_sin.sin_addr.s_addr = HostIP; */
		src_sin.sin_addr.s_addr = INADDR_ANY;
		src_sin.sin_port = htons((u_short)minportno);

		plp_block_all_signals( &oblock );
		if( UID_root ) (void)To_root();

		/* we do the next without interrupts */
		status = bind(sock, (struct sockaddr *)&src_sin, sizeof(src_sin));

		/* set up the 'resuse' flag on socket, or you may not be
			able to reuse a port for up to 10 minutes */
		if( minportno && status >= 0 ) status = Link_setreuse( sock );

		if( UID_root ) (void)To_uid( euid );
		plp_unblock_all_signals( &oblock );

		DEBUGF(DNW2) ("getconnection: sock %d, port %d, bind and reuse status %d",
			sock, minportno, status );

		if( status < 0 ){
			if( sock > 0 ) close( sock );
			sock = LINK_OPEN_FAIL;
			continue;
		}

		/*
		 * set up timeout and then make connect call
		 */
		errno = 0;
		if( UID_root ) (void)To_root();
		status = connect_timeout(timeout,sock,
			(struct sockaddr *) &dest_sin, sizeof(dest_sin));
		if( UID_root ) (void)To_uid( euid );

		DEBUGF(DNW2)("getconnection: connect sock %d, status %d, errno %d",
			sock, status, err );
		if( status < 0 || Alarm_timed_out ){
			if( Alarm_timed_out ) {
				DEBUGF(DNW1)("getconnection: connection to '%s' timed out",
					hostname);
			} else {
				DEBUGF(DNW1)("getconnection: connection to '%s' failed '%s'",
					hostname, Errormsg(err) );
			}
			(void) close (sock);
			sock = LINK_OPEN_FAIL;
		} else {
			i = sizeof( src_sin );
			if( getsockname( sock, (struct sockaddr *)&src_sin, &i ) < 0 ){
				logerr_die(LOG_ERR,"getconnnection: getsockname failed" );
			}
			DEBUGF(DNW1)( "getconnection: src ip %s, port %d, dest ip %s, port %d\n",
				inet_ntoa( src_sin.sin_addr ), ntohs( src_sin.sin_port ),
				inet_ntoa( dest_sin.sin_addr ), ntohs( dest_sin.sin_port ) );
		}
	} while( sock < 0 && minportno && minportno++ < maxportno );
	DEBUGF(DNW1)( "getconnection: socket returned %d", sock );
	return (sock);
}
/*
 * int Link_listen()
 *  1. opens a socket on the current host
 *  2. set the REUSE option on socket
 *  3. does a bind to port determined by Link_dest_port_num();
 */

int Link_listen( void )
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
	sin.sin_family = AF_Protocol;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = Link_dest_port_num();
	port = ntohs( sin.sin_port );
	DEBUGF(DNW4)("Link_listen: lpd port %d", port );

	euid = geteuid();
	if( UID_root ) (void)To_root();
	errno = 0;
	status = (sock = socket (AF_Protocol, SOCK_STREAM, 0)) < 0
		|| Link_setreuse( sock ) < 0
		|| bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0;
	err = errno;
	if( UID_root ) (void)To_uid( euid );
	if( status ){
		DEBUGF(DNW4)("Link_listen: bind to lpd port %d failed '%s'",
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
	DEBUGF(DNW4)("Link_listen: port %d, socket %d", ntohs( sin.sin_port ), sock);
	errno = err;
	return (sock);
}

/***************************************************************************
 * int Link_open(char *host, int retry, int timeout );
 ***************************************************************************/

int Link_open(char *host, int retry, int timeout )
{
	int sock;
	DEBUGF(DNW4) ("Link_open: host '%s', retry %d, timeout %d",host,retry,timeout);
	sock = Link_open_type( host, retry, timeout, Destination_port, SOCK_STREAM );
	DEBUGF(DNW4) ("Link_open: socket %d", sock );
	return(sock);
}

int Link_open_type(char *host, int retry, int timeout, int port,
	int connection_type )
{
	int sock;
	int oldport = Destination_port;
	Destination_port = port;
	DEBUGF(DNW4)(
		"Link_open_type: host '%s', retry %d, timeout %d, port %d, type %d",
		host,retry,timeout, port, connection_type );
	do {
		sock = getconnection( host, timeout, connection_type );
	} while( sock < 0 && retry-- > 0  );
	Destination_port = oldport;
	DEBUGF(DNW4) ("Link_open_type: socket %d", sock );
	return( sock );
}

/***************************************************************************
 * void Link_close( int socket )
 *    closes the link to the remote host
 ***************************************************************************/

void Link_close( int *sock )
{
	DEBUGF(DNW4) ("Link_close: closing socket %d", *sock );
	if( *sock >= 0 ){
		(void)close(*sock);
	}
	*sock = -1;
}

int Link_ack( char *host, int *socket, int timeout, int sendc, int *ack )
{
	int status, len = 0;
	char buffer[2];

	DEBUGF(DNW1)("Link_ack: host '%s' sendc 0x%x, ack 0x%x",host, sendc, ack );
	buffer[0] = 0;
	buffer[1] = 0;
	if( sendc ){
		len = 1;
		buffer[0] = sendc;
	}
	status = Link_send( host, socket, timeout, buffer, len, ack );
	DEBUGF(DNW1)("Link_ack: status %d", status );
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
	char *send, int count, int *ack )
{
	int i;      /* Watch out for longjmp * ACME Integers, Inc. */
	int status;		/* return status */
	int err = 0;

	/*
	 * set up initial conditions
	 */
	i = status = 0;	/* shut up GCC */
	if(*socket < 0) {
		DEBUGF(DNW1)( "Link_send: bad socket" );
		return( LINK_TRANSFER_FAIL );
	}
	if( ack ){
		*ack = 0;
	}

	DEBUGF(DNW1)( "Link_send: host '%s' socket %d, timeout %d",
		host, *socket, timeout );
	DEBUGF(DNW1)( "Link_send: str '%s', count %d, ack 0x%x",
		send, count , ack );

	/*
	 * set up timeout and then write
	 */
	
	i = Write_fd_len_timeout( timeout, *socket, send, count );

	/* now decode the results */
	DEBUGF(DNW3)("Link_send: final write status %d", i );
	if( i < 0 || Alarm_timed_out ){
		if( Alarm_timed_out ){
			DEBUGF(DNW3)("Link_send: write to '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else {
			DEBUGF(DNW3)("Link_send: write to '%s' failed '%s'",
				host, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
		}
	}

	/* check for an ACK to be received */
	if( status == 0 && ack ){
		char buffer[1];

		DEBUGF(DNW3)("Link_send: ack required" );
		buffer[0] = 0;
		i = Read_fd_len_timeout(timeout, *socket, buffer, 1 );
		Clear_timeout();
		err = errno;

		if( i <= 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DEBUGF(DNW3)("Link_send: ack read from '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
				DEBUGF(DNW3)("Link_send: ack read from '%s' failed - %s",
					host, Errormsg(err) );
				status = LINK_TRANSFER_FAIL;
			}
		} else if( buffer[0] ){
			*ack = buffer[0];
			status = LINK_ACK_FAIL;
		}
		DEBUGF(DNW3)("Link_send: ack read status %d, operation status %s, ack=%s",
			i, Link_err_str(status), Ack_err_str(*ack) );
		if( status == 0 && *ack == 0 ){
			/* check to see if you have some additional stuff pending */
			fd_set readfds;
			struct timeval delay;

			memset( &delay,0,sizeof(delay));
			delay.tv_usec = 1000;
			FD_ZERO( &readfds );
			FD_SET( *socket, &readfds );
			i = select( (*socket)+1,
				FD_SET_FIX((fd_set *))&readfds,
				FD_SET_FIX((fd_set *))0,
				FD_SET_FIX((fd_set *))0, &delay );
			if( i > 0 ){
				log( LOG_ERR,
				"Link_send: PROTOCOL ERROR - pending input from '%s' after ACK received",
				host );
			}
		}
	}
	if( status && status != LINK_ACK_FAIL ){
		Link_close( socket );
	}
	DEBUGF(DNW1)("Link_send: final status %s", Link_err_str(status) );
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
	int len;              /* ACME Integer, Inc. */
	int status;				/* status of operation */
	int count;	/* might be clobbered by longjmp */
	int err;					/* saved error status */

	count = pcount;
	len = status = 0;	/* shut up GCC */
	DEBUGF(DNW4)("Link_copy: sending %d of '%s' to %s, rdtmo %d, wrtmo %d, fd %d",
		count, src, host, readtimeout, writetimeout, fd );
	/* check for valid socket */
	if(*socket < 0) {
		DEBUGF(DNW4)( "Link_copy: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/* do the read */
	while( status == 0 && count != 0 ){
		len = count;
		if( count < 0 || len > sizeof(buf) ) len = sizeof(buf);
		/* do the read with timeout */
		len = Read_fd_len_timeout( readtimeout, fd, buf, len );
		err = errno;

		if( Alarm_timed_out || len <= 0 ){
			/* EOF on input */
			if( count > 0 ){
				DEBUGF(DNW4)(
					"Link_copy: read from '%s' failed, %d bytes left - %s",
					src, count, Errormsg(err) );
				status = LINK_TRANSFER_FAIL;
			} else {
				DEBUGF(DNW4)("Link_copy: read status %d count %d", len, count );
				status = 0;
			}
			break;
		}
		if( count > 0 ){
			count -= len;
		}

		DEBUGF(DNW4)("Link_copy: read %d bytes", len );
		len = Write_fd_len_timeout(writetimeout, *socket, buf, len );

		DEBUGF(DNW4)("Link_copy: write done, status %d", len );
		if( len < 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DEBUGF(DNW4)("Link_copy: write to '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
				DEBUGF(DNW4)("Link_copy: write to '%s' failed - %s",
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
		i = select( *socket+1,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, &delay );
		if( i != 0 ){
			log( LOG_ERR,
			"Link_copy: PROTOCOL ERROR - pending input from '%s' after transfer",
			host );
		}
	}
	DEBUGF(DNW4)("Link_copy: status %d", status );
	return( status );
}


/***************************************************************************
 * Link_dest_port_num () {
 * Get the destination port number
 * look up the service in the service directory using getservent
 ***************************************************************************/
int Link_dest_port_num( void )
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
		DEBUGF(DNW4)("getservbyname(\"%s\",tcp) failed", s);
		/* try integer value */
		port_num = htons( atoi( s ) );
	} else {
		port_num = sp->s_port;
	}
	DEBUGF(DNW4)("Link_dest_port_num: port %s = %d", s, ntohs( port_num ) );
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
	int i, len, max, err = 0;	/* ACME Integer, Inc. */
	int status;				/* status of operation */
	char *s = 0;				/* location of \n */

	len = i = status = 0;	/* shut up GCC */
	DEBUGF(DNW4) ("Link_line_read: reading %d from '%s' on %d, timeout %d",
		*count, host, *socket, timeout );
	/* check for valid socket */
	if(*socket < 0) {
		DEBUGF(DNW4)("Link_line_read: bad socket" );
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
		i = Read_fd_len_timeout(timeout, *socket, &buf[len], max - len );
		err = errno;
		if( i <= 0 || Alarm_timed_out ){
			break;
		}
		len += i;
		buf[len] = 0;
		DEBUGF(DNW4)("Link_line_read: len %d, last read %d, last '0x%02x' '%s'",
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
		DEBUGF(DNW4)( "Link_line_read: read from '%s' timed out", host);
		status = LINK_TRANSFER_FAIL;
	} else if( i == 0 ){
		DEBUGF(DNW4)("Link_line_read: EOF from '%s'", host );
		status = LINK_TRANSFER_FAIL;
	} else if( i < 0 ){
		DEBUGF(DNW4)("Link_line_read: read from '%s' failed - %s", host,
			Errormsg(err) );
		status = LINK_TRANSFER_FAIL;
	} else if( s == 0 ){
		DEBUGF(DNW4)("Link_line_read: no LF on line from '%s'", host );
		status = LINK_LONG_LINE_FAIL;
	} else {
		*s++ = 0;
		DEBUGF(DNW4)("Link_line_read: len %d, LF '%d'", len, s-buf );
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
		i = select( *socket+1,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, &delay );
		if( i != 0 ){
			log( LOG_ERR,
			"Link_line_read: PROTOCOL ERROR - pending input from '%s' after transfer",
			host );
		}
	}

	DEBUGF(DNW4)("Link_line_read: status %d", status );
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
	int len, i, status;      /* ACME Integer, Inc. */
	char *str;				/* input buffer pointer */
	int err;

	status = 0;	/* shut up GCC */
	DEBUGF(DNW1) ("Link_read: reading %d from '%s' on socket %d",
		*count, host, *socket );
	/* check for valid socket */
	if(*socket < 0) {
		DEBUGF(DNW1)( "Link_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	if( *count < 0 ) *count = 0;
	len = *count;
	*count = 0;
	str = buf;
	/*
	 * set up timeout and then do operation
	 */
	i = Read_fd_len_timeout(timeout, *socket, str, len );
	err = errno;
	if( i > 0 ){
		*count = i;
	}
	DEBUGFC(DNW3){
		char shortpart[32];
		int j;
		shortpart[0] = 0;
		if( i > 0 ){
			j = sizeof(shortpart) -1;
			if( i < j ) j = i;
			strncpy( shortpart, str, j ); 
			shortpart[j] = 0;
		}
		logDebug( "Link_read: wanted %d, got %d, start='%s'",
			len, i, shortpart );
	}

	if( Alarm_timed_out ){
		DEBUGF(DNW3)("Link_read: read %d from '%s' timed out",
			len, host, i );
		status = LINK_TRANSFER_FAIL;
	} else if( i < 0 ) {
		DEBUGF(DNW3)("Link_read: read %d from '%s' failed, returned %d - %s",
			len, host, i, Errormsg(err) );
		status = LINK_TRANSFER_FAIL;
	}

	if( status ){
		Link_close( socket );
	}

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
	int i, l, len, cnt;			/* number to read or write */
	int status;				/* status of operation */
	int err;					/* error */

	len = i = status = cnt = 0;	/* shut up GCC */
	DEBUGF(DNW1) ("Link_file_read: reading %d from '%s' on %d",
		*count, host, *socket );
	/* check for valid socket */
	if(*socket < 0) {
		DEBUGF(DNW2)( "Link_file_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do the transfer
	 */

	/* do the read */
	len = *count;
	while( status == 0 && len > 0 ){
		DEBUGF(DNW2)("Link_file_read: doing data read" );
		l = sizeof(str);
		if( l > len ) l = len;
		i = Read_fd_len_timeout( readtimeout, *socket, str, l );
		err = errno;
		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: len %d, readlen %d, read %d", len, l, i );
			len -= i;
			cnt = Write_fd_len_timeout(writetimeout, fd, str, i );
			err = errno;
			if( Alarm_timed_out || cnt < 0 ){
				DEBUGF(DNW2)( "Link_file_read: write %d to fd %d failed - %s",
					i, fd, Errormsg(err) );
				status = LINK_TRANSFER_FAIL; 
			}
		} else {
			DEBUGF(DNW2)("Link_file_read: read from '%s' failed - %s",
				host, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
		}
	}
	*count -= len;

	if( status == 0 && ack ){
		DEBUGF(DNW2)("Link_file_read: doing ACK read" );
		i = Read_fd_len_timeout(readtimeout, *socket, str, 1 );
		err = errno;

		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: ack read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: ACK read count %d value %d", i, *str );
			*ack = *str;
			if( *ack ){
				DEBUGF(DNW2)( "Link_file_read: nonzero ack '%d'", *ack );
				status = LINK_ACK_FAIL;
			}
		} else {
			DEBUGF(DNW2)("Link_file_read: ack read from '%s' failed - %s",
				len, host, i, Errormsg(err) );
			status = LINK_TRANSFER_FAIL;
		}
		if( status == 0 ){
			/* check to see if you have some additional stuff pending */
			fd_set readfds;
			struct timeval delay;

			memset( &delay,0,sizeof(delay));
			FD_ZERO( &readfds );
			FD_SET( *socket, &readfds );
			i = select( *socket+1,
				FD_SET_FIX((fd_set *))&readfds,
				FD_SET_FIX((fd_set *))0,
				FD_SET_FIX((fd_set *))0, &delay );
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

	DEBUGF(DNW2)("Link_file_read: status %d", status );
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
