/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: readstatus.h,v 1.30 2002/05/06 01:06:43 papowell Exp $
 ***************************************************************************/



#ifndef _READSTATUS_H_
#define _READSTATUS_H_ 1



/* PROTOTYPES */

int Read_status_info( char *host, int sock,
    int output, int timeout, int displayformat,
    int longformat, int status_line_count, int lp_mode );


int Pr_status_check( char *name );
void Pr_status_clear( void );

#endif
