/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: accounting.h,v 5.4 2000/12/25 01:51:16 papowell Exp papowell $
 ***************************************************************************/



#ifndef _ACCOUNTING_H_
#define _ACCOUNTING_H_ 1

/* PROTOTYPES */

int Setup_accounting( struct job *job );
int Do_accounting( int end, char *command, struct job *job,
	int timeout );

#endif
