/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: readstatus.h
 * PURPOSE: readstatus.c functions
 * $Id: readstatus.h,v 3.1 1996/12/28 21:40:33 papowell Exp $
 **************************************************************************/

#ifndef _READSTATUS_H
#define _READSTATUS_H



/***************************************************************************
 * Read_status: read status information from remote end
 ***************************************************************************/

int Read_status_info( char *printer, int ack_needed, int sock,
	char *host, int output );

int Pr_status_check( char *name );

#endif
