/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPD_LOGGER_H_
#define _LPD_LOGGER_H_ 1

/* PROTOTYPES */
int Start_logger( int log_fd );
void Logger( struct line_list *args );

#endif
