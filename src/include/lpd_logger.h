/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpd_logger.h,v 1.4 2002/02/09 03:37:38 papowell Exp $
 ***************************************************************************/



#ifndef _LPD_LOGGER_H_
#define _LPD_LOGGER_H_ 1

/* PROTOTYPES */
int Start_logger( int log_fd );
int Dump_queue_status(int outfd);
void Logger( struct line_list *args );

#endif
