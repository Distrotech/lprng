/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setstatus.h
 * PURPOSE: setstatus.c functions
 * $Id: setstatus.h,v 3.1 1996/12/28 21:40:35 papowell Exp $
 **************************************************************************/

#ifndef _SETSTATUS_H
#define _SETSTATUS_H


/*****************************************************************
 * Status reporting function
 *  Note that this can be a logging routine to STDOUT or to
 *  a log file depending on system that is using it.
 *****************************************************************/
    
#define NORMAL (0)
#ifdef HAVE_STDARGS
void setstatus( struct control_file *cfp, char *fmt, ... );
void setmessage( struct control_file *cfp, char *msg, char *fmt, ... );
#else 
void setstatus( va_alist );
void setmessage( va_alist );  
#endif
void send_to_logger( char *msg );
void reset_logging( void );
void Dup_logger_fd( int fd );

#endif
