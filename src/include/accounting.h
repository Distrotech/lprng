/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: accounting.h,v 1.23 2001/09/29 22:28:54 papowell Exp $
 ***************************************************************************/



#ifndef _ACCOUNTING_H_
#define _ACCOUNTING_H_ 1

/* PROTOTYPES */

int Setup_accounting( struct job *job );
int Do_accounting( int end, char *command, struct job *job,
	int timeout );

#endif
