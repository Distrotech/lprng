/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: errormsg.h
 * PURPOSE: identifies error message information, see errormsg.c
 * $Id: errormsg.h,v 3.0 1996/05/19 04:06:19 papowell Exp $
 **************************************************************************/

#ifndef _ERRORMSG_H
#define _ERRORMSG_H

#if defined(HAVE_STDARGS)
void log (int kind, char *msg,...);
void fatal (int kind, char *msg,...);
void logerr (int kind, char *msg,...);
void logerr_die (int kind, char *msg,...);
void Diemsg (char *msg,...);
void Warnmsg (char *msg,...);
void logDebug (char *msg,...);
#else
void log ();
void fatal ();
void logerr ();
void logerr_die ();
void Diemsg ();
void Warnmsg ();
void logDebug ();
#endif

const char * Errormsg ( int err );
void killchildren( int signal, pid_t pid );
int dofork();
void removepid( pid_t pid );
typedef void (*exit_ret)( void *p );
int register_exit( exit_ret exit, void *p );
void remove_exit( int i );
plp_signal_t cleanup (int signal);
void Malloc_failed( unsigned size );
char *Time_str(int shortform, time_t t);
void dump_parms( char *title, struct keywords *k );
void dump_config_list( char *title, struct keywords **list );
void dump_data_file( char *title,  struct data_file *list );
void dump_data_file_list( char *title,  struct data_file *list, int count );
void dump_control_file( char *title,  struct control_file *cf );
void dump_control_file_list( char *title,  struct control_file **cf );
void dump_filter( char *title,  struct filter *filter );

#endif
