/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpd_logger.h,v 1.36 2002/08/06 19:14:16 papowell Exp $
 ***************************************************************************/



#ifndef _LPD_LOGGER_H_
#define _LPD_LOGGER_H_ 1

/* PROTOTYPES */
int Start_logger( int log_fd );
int Dump_queue_status(int outfd);
void Logger( struct line_list *args );

#endif
