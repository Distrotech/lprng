/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendauth.h
 * PURPOSE: sendauth.c functions
 * sendauth.h,v 3.2 1997/01/19 14:34:56 papowell Exp
 **************************************************************************/

#ifndef _SENDAUTH_H
#define _SENDAUTH_H

/***************************************************************************
 * Send_auth_command: send authenticated command
 ***************************************************************************/



int Send_auth_transfer(int logtransfer, char *printer, char *host,
    int *sock, int transfer_timeout, struct printcap_entry *printcap_entry,
    int tempfd, char *tempfilename, int output, char *printfile );


int Send_auth_command( char *printer, char *host, int *sock,
    int transfer_timeout, char *line, int output );

void Fix_auth();

#endif
