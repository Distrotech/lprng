/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendlpc.h
 * PURPOSE: sendlpc.c functions
 * sendlpc.h,v 3.1 1996/12/28 21:40:34 papowell Exp
 **************************************************************************/

#ifndef _SENDLPC_H
#define _SENDLPC_H



/***************************************************************************
 * Send_lpcrequest: open a connection for lpc action
 ***************************************************************************/

void Send_lpcrequest(
	char *user,					/* user name */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output,					/* output file descriptor */
	int action					/* action */
	);


#endif
