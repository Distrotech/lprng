/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: sendreq.h,v 1.19 2001/09/18 01:43:47 papowell Exp $
 ***************************************************************************/



#ifndef _SENDREQ_H_
#define _SENDREQ_H_ 1

/* PROTOTYPES */
int Send_request(
	int class,					/* 'Q'= LPQ, 'C'= LPC, M = lprm */
	int format,					/* X for option */
	char **options,				/* options to send */
	int connect_tmout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	);

#endif
