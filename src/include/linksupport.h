/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: linksupport.h,v 5.6 2000/12/25 01:51:19 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LINKSUPPORT_H_
#define _LINKSUPPORT_H_ 1

#if !defined(HAVE_INET_NTOP)
 const char *inet_ntop( int family, const void *addr,
	char *str, size_t len );
#endif

/* PROTOTYPES */
int Link_setreuse( int sock );
int Link_setkeepalive( int sock );
int connect_timeout( int timeout,
	int sock, struct sockaddr *name, int namelen);
int getconnection ( char *hostname, char *dest_port,
	int timeout, int connection_type, struct sockaddr *bindto );
void Set_linger( int sock, int n );
int Link_listen( void );
int Link_open(char *host, char *port, int timeout, struct sockaddr *bindto );
int Link_open_type(char *host, char *port, int timeout, int connection_type,
	struct sockaddr *bindto );
int Link_open_list( char *hostlist, char **result,
	char *port, int timeout, struct sockaddr *bindto );
void Link_close( int *sock );
int Link_send( char *host, int *sock, int timeout,
	char *sendstr, int count, int *ack );
int Link_copy( char *host, int *sock, int readtimeout, int writetimeout,
	char *src, int fd, double pcount);
int Link_dest_port_num( char *port );
int Link_line_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_file_read(char *host, int *sock, int readtimeout, int writetimeout,
	  int fd, double *count, int *ack );
const char *Link_err_str (int n);
const char *Ack_err_str (int n);
int AF_Protocol(void);
int inet_pton( int family, const char *strptr, void *addr );
const char *inet_ntop( int family, const void *addr,
	char *str, size_t len );
const char *inet_ntop_sockaddr( struct sockaddr *addr,
	char *str, int len );

#endif