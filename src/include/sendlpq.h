/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendstatus.h
 * PURPOSE: sendstatus.c functions
 * sendlpq.h,v 3.1 1996/12/28 21:40:35 papowell Exp
 **************************************************************************/

#ifndef _SENDLPQ_H
#define _SENDLPQ_H



/***************************************************************************
 * Send_lpqrequest: open a connection for status information
 ***************************************************************************/

void Send_lpqrequest( char *remoteprinter, char *host, int format,
	char **options,	int connect_timeout, int transfer_timeout,
	int output );


#endif
