/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: linksupport.h
 * PURPOSE: linksupport.c functions
 * linksupport.h,v 3.5 1998/01/08 09:51:25 papowell Exp
 **************************************************************************/

#ifndef _LINKSUPPORT_H
#define _LINKSUPPORT_H


/*****************************************************************
 * Connection support code
 *  See extensive comments in link_support.c
 *****************************************************************/

int Link_setreuse( int sock );
int Link_getreuse( int sock );
int Link_dest_port_num( void );
int Link_listen(void);
int Link_open(char *host, int timeout );
int Link_open_type(char *host, int timeout,
	int port, int connection_type );
void Link_close( int *sock );
int Link_ack( char *host, int *socket, int timeout, int sendc, int *ack );
int Link_send ( char *host, int *socket, int timeout,
    char *send, int count, int *ack );
int Link_copy( char *host, int *socket, int readtimeout, int writetimeout,
    char *src, int fd, int count);
int Link_get( char *host, int *socket, int timeout, char *dest, FILE *fp );
int Link_line_read(char *host, int *socket, int timeout,
      char *buf, int *count );
int Link_read(char *host, int *socket, int timeout,
      char *buf, int *count );
int Link_file_read(char *host, int *socket, int readtimeout, int writetimeout,
      int fd, int *count, int *ack );
const char *Link_err_str (int n);
const char *Ack_err_str (int n);
void Set_linger( int sock, int n );

#endif
