/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlprm.h
 * PURPOSE: sendlprm.c functions
 * sendlprm.h,v 3.1 1996/12/28 21:40:35 papowell Exp
 **************************************************************************/

#ifndef _SENDLPRM_H
#define _SENDLPRM_H



/***************************************************************************
 * Send_lprmrequest: open a connection for lprm action
 ***************************************************************************/

void Send_lprmrequest( char *remoteprinter, char *host, char *logname,
	char **options,	int connect_timeout, int transfer_timeout,
	int output );



#endif
